#include "../include/TcpServer.h"
#include"../include/LRU.h"
#include <cstring>

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

    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0)
    {
        perror("epoll");
        return false;
    }
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    cout << "Server started on port 8080" << endl;
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
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
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
    clients[fd]={fd,""};
    //cout<<"客户端"<<fd<<"成功连接！"<<endl;
    return true;
}

void Tcp::handle_read(const int &fd, VectorCache &cache,ThreadPool& pool)
{
    std::unique_lock<mutex> lk(n_mtx);
    //cout<<"正在处理读事件\n";
    auto it = clients.find(fd);
    if (it == clients.end())
        return;

    Client &client = it->second;
    char buf[1024];
    bool connection_closed = false;
    //cout<<"正在读取数据！"<<endl;
    // 1. 集中读取数据
    while (true)
    {
        int n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0)
        {
            client.readBuf.append(buf, n);
        }
        else if (n == 0)
        {
            connection_closed = true;
            break;
        }
        else
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            connection_closed = true;
            break;
        }
    }
    // 2. 无论连接是否关闭，先处理缓冲区里剩下的完整指令
    //cout<<"读取到数据！"<<endl;
    lk.unlock();
    size_t pos;
    //*************************************多线程*************************************
    while ((pos = client.readBuf.find('\n')) != string::npos)
    {
        string request = client.readBuf.substr(0, pos);
        client.readBuf.erase(0, pos + 1);
        //cout<<request<<endl;
        pool.enqueue([request = std::move(request), &cache, fd]() mutable
                     {
            size_t last = request.find_last_not_of("\n\r\t");
            if (last != string::npos)
        {
            request.erase(last + 1);
            //cout<<request<<endl;
            cache.handleRequest(request,fd); // 处理逻辑
        } });
    } 

    // 3. 最后统一执行清理逻辑
    if (connection_closed)
    {
        std::lock_guard<mutex> lk(n_mtx);
        if(clients.erase(fd)>0){
            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
            close(fd);
            //cout<<"客户端"<<std::to_string(fd)<<" 已经关闭!\n";
        }
    }
}

void Tcp::run()
{
    VectorCache myCache(1000000);
    ThreadPool pool;
    pool.enqueue([&myCache]() mutable {
        myCache.loadBin();
    });
    {
        std::lock_guard<mutex> lk(n_mtx);
        cout << "数据热启动中！......" << endl;
    }
    if(!init()){
        return;
    }
    while (true)
    {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENT, -1);
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

