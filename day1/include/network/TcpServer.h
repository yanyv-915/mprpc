#pragma once
#include"../cache/LRU.h"
#include"../core/ThreadPool.h"
#include"Protocol.h"
#include <sys/epoll.h>

#define MAX_EVENT 1024
#define PORT 8080

class Tcp{
private:
    int listen_fd,epfd;
    std::unordered_map<size_t,Client>clients;
    epoll_event ev,events[MAX_EVENT];
    mutex n_mtx;
public:
    bool init();
    void setNonBlocking(const int& fd);
    bool accept_client(int& client_fd);
    void add_epoll(int fd,uint32_t& event);
    bool add_client(int& fd);
    void handle_read(const int& fd,VectorCache& cache,ThreadPool& pool);
    void run();
};