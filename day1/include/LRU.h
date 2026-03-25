#pragma once
#include"Header.h"
#include"io.h"
#include<atomic>
class VectorCache{
private:
    struct Node{
        std::string key;
        VectorData data;
    };
    
    size_t capacity;
    std::list<Node> cacheList;
    std::unordered_map<std::string,std::list<Node>::iterator>cacheMap;
    mutex l_mtx;
    mutex aof_mtx;
    std::atomic<bool>is_loading{true};
    std::ofstream aof_file;
    std::filesystem::path aof_path;
    int global_dim=-1;

    static std::filesystem::path defaultAofPath();
    VectorData parseVector(string vecStr);
    void exe_set(const string& request);
    void exe_get(const string& request,const int& client_fd);
    string exe_search(const string& request);
    void executeCommand(const string& cmd,const int& client_fd);
    bool checkDim(const VectorData& vec);
    float calL2(const VectorData& v1,const VectorData& v2);
public:
    
    
    VectorCache(size_t cap);
    ~VectorCache();
    bool get(const std::string& key,VectorData& outData);
    void put(const std::string& key,const VectorData& data);
    void handleRequest(const string& rawRequest,const int& client_fd);
    void loadBin();
    void saveToBin(const string& request);
    void print();
};