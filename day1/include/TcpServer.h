#pragma once
#include"Header.h"
#include"LRU.h"
#include"Command.h"
#include"ThreadPool.h"

#define MAX_EVENT 1024
#define PORT 8080

class Tcp{
private:
    int listen_fd,epoll_fd;
    std::unordered_map<int,Client>clients;
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