#include "../include/ThreadPool.h"
ThreadPool::ThreadPool(size_t threads) : stop(false)
{
    for (size_t i = 0; i < threads; i++)
    {
        worker.emplace_back([this]()
                            {
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
    }
}

ThreadPool::~ThreadPool()
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
