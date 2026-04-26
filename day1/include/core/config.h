#pragma once
#include <string>
#include <yaml-cpp/yaml.h>

class Config {
public:
    static Config& getInstance();
    void load(const std::string& filename);

    int port;
    size_t aof_threshold;
    size_t segment_cnt;
    int thread_pool_size; // 新增

private:
    Config() = default;
};