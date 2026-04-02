#pragma once
#include"../cache/Vector.h"
#include"../cache/LRU.h"
#include"../core/IVectorData.h"
#include"../network/Protocol.h"
#include"VectorFactory.h"
#include"io.h"

#include<string>
#include<queue>
#include<vector>
#include<mutex>
#include<atomic>
#include<condition_variable>
#include<thread>
#include<fstream>
using std::string;
using std::vector;
using std::mutex;
struct AofTask{
    MessageHeader header;
    std::shared_ptr<IVectorData> data;
};

class VectorCache;
class AofManager{
private:
    vector<AofTask> activa_buffer;
    vector<AofTask> persist_buffer;
    mutex aof_mtx;
    std::condition_variable cv;
    std::thread worker;
    std::atomic<bool> stop=false;
    size_t buffer_threshold=5000;
    void work_loop();
    void asyncPush(const MessageHeader& header,const std::shared_ptr<IVectorData>& vec);
private:
    string filename;
    std::ofstream aof_file;

public:
    AofManager(const string& path);
    AofManager();
    ~AofManager();
    void appendSet(const uint64_t& keyId,const std::shared_ptr<IVectorData>& vec);
    void appendDel(const uint64_t& keyId);
    void recover(VectorCache& cache);
};