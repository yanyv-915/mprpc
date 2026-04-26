#pragma once
#include"../network/Protocol.h"

#include<shared_mutex>
#include<iostream>
#include <sys/epoll.h>
#include <atomic>
#include <csignal>
// 定义一个全局标志位
static std::atomic<bool> g_running(true);
using std::vector;
#define MAX_EVENT 1024
#define PORT 8080

class VectorCache;
class AofManager;
class ThreadPool;
class Tcp{
private:
    size_t SEGMENT_CNT=16;
    size_t MASK=SEGMENT_CNT-1;
    struct Segment{
        std::shared_mutex mtx;
        std::unordered_map<size_t,std::shared_ptr<Client>>clients;
    };
    vector<Segment>segments;
public:
    // 在 TcpServer 类中初始化
    Tcp():segments(SEGMENT_CNT) {}

    // 获取 Client 的安全方式
    std::shared_ptr<Client> get_client(int fd) {
        size_t idx = fd & MASK;
        std::shared_lock lk(segments[idx].mtx); // 读锁，多个线程可以同时获取不同或相同的 Client
        auto it = segments[idx].clients.find(fd);
        return (it != segments[idx].clients.end()) ? it->second : nullptr;
    }
    // 删除 Client 的安全方式
    void remove_client(int fd) {
        size_t idx = fd & MASK;
        std::unique_lock lk(segments[idx].mtx); // 写锁，确保删除时的排他性
        segments[idx].clients.erase(fd);
    }
private:
    std::atomic<uint64_t> total_qps;
    std::atomic<uint64_t> search_latency_ms;
    std::atomic<uint64_t> active_connections;
private:
    int listen_fd,epfd;
    
    epoll_event ev,events[MAX_EVENT];
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