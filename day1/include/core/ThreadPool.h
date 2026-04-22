#pragma once
#include <thread>
#include<iostream>
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
    ThreadPool(size_t threads=thread::hardware_concurrency()) : stop(false)
    {
        for (size_t i = 0; i < threads; i++)
        {
            try{
                worker.emplace_back([this](){
                        while(true){
                            function<void()> task;
                            std::unique_lock<mutex> lk(t_mtx);
                            cv.wait(lk,[this](){
                                return stop||!tasks.empty();
                            });
                            if(stop&&tasks.empty()){
                                return;
                            }
                            task=move(tasks.front());
                            tasks.pop();
                            lk.unlock();
                            task();
                        } });
            
            //std::cout << "正在创建线程 " << i << "..." << std::endl;
            }
            catch(const std::exception& e){
                std::cerr << "[POOL] 线程创建失败: " << e.what() << std::endl;
            }
            
        }
        std::cout<<"线程池初始化成功！\n";
    }

    ~ThreadPool()
    {
        std::unique_lock<mutex> lk(t_mtx);
        stop = true;
        cv.notify_all();
        lk.unlock();
        for (auto &t : worker)
        {
            if (t.joinable())
                t.join();
        }
    }
    template <typename F>
    void enqueue(F &&f)
    {
        std::unique_lock<mutex> lk(t_mtx);
        tasks.emplace(std::forward<F>(f));
        lk.unlock();
        cv.notify_one();
    }
};