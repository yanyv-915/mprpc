#pragma once
#include<io.h>
#include<IVectorData.h>
#include<Protocol.h>
#include<atomic>
#include<thread>
#include<mutex>
#include<shared_mutex>
#include<condition_variable>
#include<memory>
#include<vector>
#include<list>
#include<unordered_map>
#include<fstream>

using std::vector;
using std::list;
using std::unordered_map;
using std::mutex;
using std::thread;
using std::atomic;
using std::shared_ptr;

class VectorCache{
private:
    static const size_t SEGMENT_CNT=128;
    static const size_t MASK = SEGMENT_CNT-1;
    struct Segment{
        mutex mtx;
        unordered_map<uint64_t,std::shared_ptr<IVectorData>> storage;
    };
    vector<Segment>segments;
    

private:
//容器初始化
    size_t capacity;
    
    unordered_map<uint64_t,std::shared_ptr<IVectorData>> cache_map;

public:
    VectorCache(size_t cap);
    void set(const uint64_t& key,const std::shared_ptr<IVectorData>& vec);
    bool get(const uint64_t& key,std::shared_ptr<IVectorData>& vec);
    void handleRequest(const MessageHeader& header,const shared_ptr<IVectorData>& vec,const size_t& fd);
};