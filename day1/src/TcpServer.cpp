#include "TcpServer.h"
#include"LRU.h"
#include"aof.h"
#include <cstring>
#include<fcntl.h>
#include<unistd.h>
#include <iostream>
using std::cout;
using std::endl;

void Tcp::setNonBlocking(const int& fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool Tcp::init()
{   
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0)
    {
        perror("socket");
        return false;
    }
    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    

    if (bind(listen_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        int save_errno = errno;
        fprintf(stderr, "[ERROR] Bind failed! Errno: %d (%s)\n", save_errno, strerror(save_errno));
        
        // 暴力排查：看看当前进程开了多少 fd
        if (system("ls -l /proc/self/fd") == -1){
            return false;
        }
        
    }
    if (listen(listen_fd, 100) < 0)
    {
        perror("listen");
        return false;
    }
    setNonBlocking(listen_fd);

    epfd = epoll_create1(0);
    if (epfd < 0)
    {
        perror("epoll");
        return false;
    }
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);

    cout << "Server started on port 8080" << std::endl;
    return true;
}

bool Tcp::accept_client(int &client_fd)
{
    sockaddr_in client_addr{};
    socklen_t len = sizeof(client_addr);
    client_fd = accept(listen_fd, (sockaddr *)&client_addr, &len);
    if (client_fd < 0)
    {
        perror("accept");
        return false;
    }

    return true;
}

void Tcp::add_epoll(int fd,uint32_t& event)
{
    epoll_event ev;
    ev.data.fd = fd;
    ev.events = event|EPOLLET;
    setNonBlocking(fd);
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

bool Tcp::add_client(int& fd){
    if (!accept_client(fd))
    {
        perror("accept client");
        return false;
    }
    uint32_t event=EPOLLIN|EPOLLOUT;
    add_epoll(fd,event);
    std::unique_lock<mutex> lk(n_mtx);
    MessageHeader header{};
    clients[fd]={"",false,header};
    //cout<<"客户端"<<fd<<"成功连接！"<<endl;
    return true;
}

void Tcp::handle_read(const int &fd, VectorCache &cache,ThreadPool& pool)
{
    std::unique_lock<mutex> lk(n_mtx);
    auto it=clients.find(fd);
    if(it==clients.end()) return;

    Client& client=it->second;
    char buf[4096];
    bool con_close=false;

    while(true){
        int n=recv(fd,buf,sizeof(buf),0);
        if(n>0){
            client.readBuf.append(buf,n);
        }
        else if(n==0){
            con_close=true;
            break;
        }
        else{
            if(errno==EAGAIN||errno==EWOULDBLOCK) break;
            con_close=true;
            break;
        }
    }
    lk.unlock();
    const size_t HEADER_SIZE=sizeof(MessageHeader);
    while(true){
        if(!client.headerParsed){
            if(client.readBuf.size()<HEADER_SIZE) break;
            memcpy(&client.curHeader,client.readBuf.data(),HEADER_SIZE);
            if(client.curHeader.magic!=0x4647){
                std::lock_guard<mutex> lk(n_mtx);
                clients.erase(fd);
                epoll_ctl(epfd,EPOLL_CTL_DEL,fd,nullptr);
                close(fd);
                return;
            }
            client.headerParsed=true;
        }
        if(client.headerParsed){
            size_t bodySize=0;
            if(client.curHeader.op == OpCode::SET || client.curHeader.op == OpCode::DEL){
                bodySize=client.curHeader.dim*sizeof(float);
            }
            if(client.readBuf.size()<HEADER_SIZE+bodySize) break;
            auto vec=VectorFactoy::create(client.curHeader.dataType,client.curHeader.dim);
            if(bodySize>0 && vec){
                memcpy((void*)vec->getRawPtr(),client.readBuf.data()+HEADER_SIZE,bodySize);
            }
            pool.enqueue([vec,fd,&cache,&client]() mutable{
                cache.handleRequest(client.curHeader,vec,fd);
            });
            client.readBuf.erase(0,HEADER_SIZE+bodySize);
        }
    }
    // 3. 最后统一执行清理逻辑
    if (con_close)
    {

        std::lock_guard<mutex> lk(n_mtx);

        if(clients.erase(fd)>0){

            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);

            close(fd);

            //cout<<"客户端"<<std::to_string(fd)<<" 已经关闭!\n";

        }

    }
}

void Tcp::run()
{
    VectorCache myCache(1000000);
    AofManager aof("aof.bin");
    ThreadPool pool;
    if(!init()){
        return;
    }
    while (true)
    {
        int nfds = epoll_wait(epfd, events, MAX_EVENT, -1);
        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.fd == listen_fd)
            {
                int client_fd = -1;
                if(!add_client(client_fd)){
                    continue;
                }
            }
            else
            {
                int fd=events[i].data.fd;
                handle_read(fd,myCache,pool);
            }
        }
    }
    
}

