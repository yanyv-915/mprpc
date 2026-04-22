#include "../include/network/TcpServer.h"
#include"../include/cache/LRU.h"
#include"../include/utils/aof.h"
#include"../include/core/ThreadPool.h"
#include"../include/network/Protocol.h"
#include"../include/network/Protocol.h"

#include <cstring>
#include<fcntl.h>
#include<unistd.h>
#include <iostream>
#include<memory>
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
    //cout<<"listen ok\n";
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
        perror("bind");
        return false;
    }
    //cout<<"bind ok\n";
    if (listen(listen_fd, 100) < 0)
    {
        perror("listen");
        return false;
    }

    //cout<<"listen2 ok\n";
    setNonBlocking(listen_fd);

    epfd = epoll_create1(0);
    if (epfd < 0)
    {
        perror("epoll");
        return false;
    }
    //cout<<"epoll ok\n";
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
        if(errno == EAGAIN || errno == EWOULDBLOCK) return false;
        perror("accept");
        return false;
    }
    return true;
}

void Tcp::add_epoll(const int& fd,const uint32_t& event)
{
    epoll_event ev;
    ev.data.fd = fd;
    ev.events = event|EPOLLET;
    setNonBlocking(fd);
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

void Tcp::update_epoll(const int& fd,const uint32_t& event){
    epoll_event ev;
    ev.data.fd=fd;
    ev.events=event|EPOLLET;
    std::unique_lock<std::shared_mutex> lk(clients[fd]->write_mtx);
    if(epoll_ctl(epfd,EPOLL_CTL_MOD,fd,&ev) == -1){
        perror("epoll_ctl EPOLL_CTL_MOD failed");
    }
}

bool Tcp::add_client(int& fd){
    if (!accept_client(fd))
    {
        return false;
    }
    uint32_t event=EPOLLIN;
    add_epoll(fd,event);
    std::unique_lock<mutex> lk(n_mtx);
    MessageHeader header{};
    clients[fd]=std::make_shared<Client>(false,header);
    //cout<<"客户端"<<fd<<"成功连接！"<<endl;
    return true;
}

void Tcp::serializeResponseToBuf(const Response& res,shared_ptr<Client>& clientPtr){
    std::unique_lock<std::shared_mutex> lk(clientPtr->write_mtx);
    uint64_t cnt= (res.op == OpCode::SEARCH) ?res.data.size() : 0;
    MessageHeader header{};
    header.magic=0x4647;
    header.op=static_cast<OpCode> (res.op);
    header.dataType=static_cast<DataType>(res.dataType);
    header.dim=clientPtr->curHeader.dim;
    if(header.op!=OpCode::SEARCH){
        header.key_id = res.success ? 1 : 0;
        const char* headerPtr=reinterpret_cast<const char*>(&header);
        clientPtr->writeBuf.append(headerPtr,sizeof(MessageHeader));
        return;
    }
    header.key_id=cnt;
    const char* headerPtr=reinterpret_cast<const char*>(&header);
    clientPtr->writeBuf.append(headerPtr,sizeof(MessageHeader));

    if(res.op==OpCode::SEARCH){
        for(auto& vec:res.data){
            const char* dataPtr = static_cast<const char*> (vec->getRawPtr());
            clientPtr->writeBuf.append(dataPtr,vec->getSize());
        }
    }
}

void Tcp::handle_send(const int& fd){
    bool con_close=false;
    std::unique_lock<mutex> lk_global(n_mtx);
    auto it=clients.find(fd);
    if(it==clients.end()) return;
    auto clientPtr=it->second;
    lk_global.unlock();
    std::unique_lock<std::shared_mutex> lk(clientPtr->write_mtx);
    while(true){
        if(clientPtr->writeBuf.readableBytes()==0){
            break;
        }
        ssize_t n=send(fd,clientPtr->writeBuf.peek(),clientPtr->writeBuf.readableBytes(),MSG_NOSIGNAL);
        if(n>0){
            clientPtr->writeBuf.retrieve(n);
            if(clientPtr->writeBuf.readableBytes()==0){
                break;
            }
        }
        else if(n==0){
            con_close=true;
            break;
        }
        else{
            if(errno==EAGAIN||errno==EWOULDBLOCK) return;
            con_close=true;
            break;
        }
    }
    lk.unlock();
    if (con_close)
    {
        std::lock_guard<mutex> lk(n_mtx);
        if(clients.erase(fd)>0){
            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
            close(fd);
            return;
        }
    }
    else update_epoll(fd, EPOLLIN);
}

bool Tcp::praseMsg(const int& fd,shared_ptr<Client> client,VectorCache &cache,ThreadPool& pool){
    const size_t HEADER_SIZE=sizeof(MessageHeader);
    int parseCount = 0;
    while(client->readBuf.readableBytes() >= HEADER_SIZE){
        if (++parseCount > 1000) { // 如果一个循环处理了1000个包还没完，肯定出问题了
            client->readBuf.retrieveAll();
            return true; 
        }
        if(!client->headerParsed){
            if(client->readBuf.readableBytes()<HEADER_SIZE) break;
            memcpy(&client->curHeader,client->readBuf.peek(),HEADER_SIZE);
            if(client->curHeader.magic != 0x4647){
                client->readBuf.retrieveAll();
                return true;
            }
            client->headerParsed = true;
        }
        if(client->headerParsed){
            size_t bodySize=0;
            //SET,SEARCH需要知道客户端向量维度，其它选项不需要知道，只需要key_id即可
            if(client->curHeader.op == OpCode::SET || client->curHeader.op == OpCode::SEARCH){
                switch (client->curHeader.dataType){
                    case DataType::FLOAT32:
                        bodySize = client->curHeader.dim * sizeof(float);
                        break;
                    case DataType::INT16:
                        bodySize = client->curHeader.dim * sizeof(int16_t);
                        break;
                    case DataType::BINARY:
                        bodySize = client->curHeader.dim * sizeof(uint8_t);
                        break;
                    default:
                        bodySize=0;
                        break;
                }
            }
            if(client->readBuf.readableBytes()<HEADER_SIZE+bodySize) break;
            auto vec=VectorFactoy::create(client->curHeader.dataType,client->curHeader.dim);
            if(bodySize>0 && vec){
                memcpy((void*)vec->getRawPtr(),client->readBuf.peek()+HEADER_SIZE,bodySize);
            }
            auto clientPtr=client;
            MessageHeader taskHeader = client->curHeader;
            pool.enqueue([vec,fd,&cache,clientPtr,taskHeader,this]() mutable{
                Response res=cache.handleRequest(taskHeader,vec);
                serializeResponseToBuf(res,clientPtr);  
                {
                    std::lock_guard<mutex> lk_global(n_mtx);
                    if (clients.find(fd) == clients.end()) {
                        return; // 客户端已经断开并被清理了，直接放弃写回
                    }
                }
                update_epoll(fd,EPOLLIN | EPOLLOUT);
            });
            client->readBuf.retrieve(HEADER_SIZE+bodySize);
            client->headerParsed=false;
        }
    }
    return false;
}

void Tcp::handle_read(const int &fd, VectorCache &cache,ThreadPool& pool)
{
    int saveErrno = 0;
    std::unique_lock<mutex> lk_gloabal(n_mtx);
    auto it=clients.find(fd);
    if(it==clients.end()) return;
    auto client=it->second;
    lk_gloabal.unlock();
    bool con_close=false;

    std::unique_lock<std::shared_mutex> lk(client->read_mtx);
    while(true){
        ssize_t n = client->readBuf.readFd(fd,&saveErrno);
        if(n > 0){
            continue;
        }
        else if(n == 0){
            con_close=true;
            break;
        }
        else{
            if(saveErrno == EAGAIN || saveErrno == EWOULDBLOCK){
                break;
            }else if (saveErrno == ECONNRESET) {
                // 对端强制复位
                con_close = true;
            } else {
                perror("recv error");
                con_close = true;
            }
            break;
        }
    }
    con_close=praseMsg(fd,client,cache,pool);
    
    lk.unlock();
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
    // 注册信号
    std::signal(SIGINT, handle_sigint);
    VectorCache myCache(1024);
    ThreadPool pool;
    myCache.getAof()->setRewriteTrigger([&myCache](){
        myCache.getAof()->rewrite(myCache);
    });
    if(!init()){
        return;
    }
    while (g_running)
    {
        int nfds = epoll_wait(epfd, events, MAX_EVENT, -1);
        for (int i = 0; i < nfds; i++)
        {
            if (events[i].data.fd == listen_fd)
            {
                while (true){
                    int client_fd = -1;
                    if(!add_client(client_fd)){
                        break;
                    }
                }
                continue;
            }
            if(events[i].events & EPOLLIN)
            {
                int fd=events[i].data.fd;
                handle_read(fd,myCache,pool);
            }
            if(events[i].events & EPOLLOUT){
                int fd=events[i].data.fd;
                handle_send(fd);
            }
        }
    }
    
}

