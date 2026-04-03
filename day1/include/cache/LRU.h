#pragma once
#include"../utils/io.h"
#include"../core/IVectorData.h"
#include"../network/Protocol.h"
#include"../utils/aof.h"

#include<atomic>
#include<thread>
#include<mutex>
#include<shared_mutex>
#include<condition_variable>
#include<future>

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

class AofManager;
class VectorCache{
public:
//容器初始化
    VectorCache(size_t cap);
    void set(const uint64_t& key,const std::shared_ptr<IVectorData>& vec);
    void del(const uint64_t& key);
    vector<shared_ptr<IVectorData>> search(const shared_ptr<IVectorData>& query,size_t topK=1);
    void handleRequest(const MessageHeader& header,shared_ptr<IVectorData>& vec,const size_t& fd);

private:
    size_t cap_single;
    shared_ptr<AofManager>aof;
    thread file_loading;
    long global_dim=-1;
private:
//实现基础功能
    bool get(const uint64_t& key,std::shared_ptr<IVectorData>& vec);
    bool checkDim(const shared_ptr<IVectorData>& vec);
    struct SearchRes{
        shared_ptr<IVectorData> vec;
        float distance;
    };
    
private:
    static const size_t SEGMENT_CNT=128;
    static const size_t MASK = SEGMENT_CNT-1;
    struct Segment{
        std::shared_mutex mtx;
        std::list<uint64_t> cache_list;
        struct Node{
            shared_ptr<IVectorData> data;
            std::list<uint64_t>::iterator it;
        };
        unordered_map<uint64_t,Node> storage;
        
    };
    vector<Segment>segments;
};