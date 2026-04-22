#pragma once
#include"../network/Protocol.h"

#include<shared_mutex>
#include<iostream>
#include <sys/epoll.h>
#include <atomic>
#include <csignal>
// 定义一个全局标志位
static std::atomic<bool> g_running(true);
#define MAX_EVENT 1024
#define PORT 8080

class VectorCache;
class AofManager;
class ThreadPool;
class Tcp{
private:
    int listen_fd,epfd;
    std::unordered_map<size_t,std::shared_ptr<Client>>clients;
    epoll_event ev,events[MAX_EVENT];
    mutex n_mtx;
    void serializeResponseToBuf(const Response& res, std::shared_ptr<Client>& clientPtr);
public:
    static void handle_sigint(int sig) {
        if (sig == SIGINT) {
            std::cout << "\n[Server] 捕捉到退出信号，正在停止事件循环..." << std::endl;
            g_running = false;
        }
    }
    bool init();
    void setNonBlocking(const int& fd);
    bool accept_client(int& client_fd);
    void add_epoll(const int& fd,const uint32_t& event);
    void update_epoll(const int& fd,const uint32_t& event);
    bool add_client(int& fd);
    bool praseMsg(const int& fd,std::shared_ptr<Client> client,VectorCache &cache,ThreadPool& pool);
    void handle_read(const int& fd,VectorCache& cache,ThreadPool& pool);
    void handle_send(const int& fd);
    void run();
};