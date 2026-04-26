#include "../include/core/config.h"
#include <iostream>

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

void Config::load(const std::string& filename) {
    try {
        YAML::Node config = YAML::LoadFile(filename);
        
        // 建立联系：将 YAML 里的值赋予 C++ 对象的成员变量
        port = config["server"]["port"].as<int>();
        thread_pool_size = config["server"]["thread_pool_size"].as<int>();
        aof_threshold = config["storage"]["rewrite_threshold_mb"].as<size_t>() * 1024 * 1024;
        segment_cnt = config["storage"]["segment_count"].as<size_t>();
        
        std::cout << "[SUCCESS] 成功加载配置文件: " << filename << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] 配置文件读取失败: " << e.what() << std::endl;
        exit(1); // 关键：如果配置加载失败，程序不应继续运行
    }
}