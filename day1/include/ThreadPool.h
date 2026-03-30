#pragma once
#include <thread>
#include<condition_variable>
#include<mutex>
#include<queue>
#include<vector>
#include<functional>
#include<atomic>
using std::mutex;
using std::queue;
using std::vector;
using std::function;
using std::thread;
using std::atomic;

class ThreadPool{
private:
    mutex t_mtx;
    std::condition_variable cv;
    queue<function<void()>>tasks;
    vector<thread>worker;
    atomic<bool> stop;

public:
    ThreadPool(size_t threads=thread::hardware_concurrency());
    ~ThreadPool();
    template<typename F>
    void enqueue(F&& f);
};

template <typename F>
void ThreadPool::enqueue(F &&f)
{
    std::unique_lock<mutex> lk(t_mtx);
    tasks.emplace(std::forward<F>(f));
    lk.unlock();
    cv.notify_one();
}