#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include<mutex>
#include "../include/network/Protocol.h" // 确保路径正确
using std::mutex;
void check_aof(const std::string& filename) {
    std::ifstream is(filename, std::ios::binary);
    if (!is.is_open()) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return;
    }

    size_t total_count = 0;
    size_t set_count = 0;
    size_t del_count = 0;
    size_t corrupted_count = 0;
    uint32_t first_dim = 0;
    bool dim_consistent = true;

    std::cout << std::left << std::setw(10) << "Index" 
              << std::setw(10) << "Op" 
              << std::setw(20) << "KeyID" 
              << std::setw(10) << "Dim" 
              << "Status" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    while (is.peek() != EOF) {
        MessageHeader header;
        is.read(reinterpret_cast<char*>(&header), sizeof(MessageHeader));

        // 1. 检查 Magic Number
        if (header.magic != 0x4647) {
            std::cerr << "\n[错误] 发现非法 Magic Number: " << std::hex << header.magic 
                      << " 位置: " << is.tellg() << std::dec << std::endl;
            corrupted_count++;
            break; 
        }

        total_count++;
        std::string op_str = (header.op == OpCode::SET) ? "SET" : "DEL";
        if (header.op == OpCode::SET) set_count++;
        else if (header.op == OpCode::DEL) del_count++;

        // 2. 检查维度一致性
        if (header.op == OpCode::SET) {
            if (first_dim == 0) first_dim = header.dim;
            else if (header.dim != first_dim) dim_consistent = false;
        }

        // 3. 跳过或读取 Body 数据
        if (header.op == OpCode::SET) {
            size_t body_size = header.dim * sizeof(float);
            is.seekg(body_size, std::ios::cur); // 快速跳过数据部分，只检查逻辑
        }

        // 打印前 10 条和最后一条作为抽检
        if (total_count <= 10) {
            std::cout << std::left << std::setw(10) << total_count 
                      << std::setw(10) << op_str 
                      << std::setw(20) << header.key_id 
                      << std::setw(10) << header.dim 
                      << "OK" << std::endl;
        }
    }

    std::cout << std::string(60, '-') << std::endl;
    std::cout << "--- 校验报告 ---" << std::endl;
    std::cout << "总记录数:   " << total_count << std::endl;
    std::cout << "SET 操作数: " << set_count << std::endl;
    std::cout << "DEL 操作数: " << del_count << std::endl;
    std::cout << "损坏记录数: " << corrupted_count << std::endl;
    std::cout << "维度一致性: " << (dim_consistent ? "通过 (All " + std::to_string(first_dim) + ")" : "失败 (存在多种维度)") << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "用法: ./aof_checker <aof_file_path>" << std::endl;
        return 1;
    }
    check_aof(argv[1]);
    return 0;
}