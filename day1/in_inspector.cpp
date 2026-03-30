#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip> // 用于美化输出
#include "include/io.h" // 确保包含你封装的读写类

using namespace std;

int main(int argc, char* argv[]) {
    string path = "cache.aof"; // 默认路径
    if (argc > 1) path = argv[1];

    ifstream ifs(path, ios::binary);
    if (!ifs) {
        cerr << "无法打开文件: " << path << endl;
        return 1;
    }

    cout << "===== 二进制文件检测工具 =====" << endl;
    cout << "正在读取: " << path << endl;
    cout << "---------------------------" << endl;

    int count = 0;
    int global_dim = -1;

    while (ifs.peek() != EOF) {
        // 1. 读取 Key（严格检查读取是否完整）
        string key;
        if (!IO::readString(ifs, key)) {
            if (ifs.eof()) break;
            cerr << ">> [错误] Key 读取失败，文件可能在尾部发生截断。" << endl;
            break;
        }

        // 2. 读取 Vector（严格检查读取是否完整）
        VectorData vec;
        if (!IO::readVec(ifs, vec)) {
            cerr << ">> [错误] Vector 读取失败，记录可能不完整（常见于最后一条未写完）。" << endl;
            break;
        }
        
        // 3. 统计与校验
        if (global_dim == -1) global_dim = vec.size();
        
        count++;

        // 4. 打印前 5 条数据进行人工核对
        if (count <= 5) {
            cout << "[" << count << "] Key: " << key 
                 << " | Dim: " << vec.size() << " | Data: [";
            for (size_t i = 0; i < min((size_t)vec.size(), (size_t)3); ++i) {
                cout << vec[i] << (i == min((size_t)vec.size(), (size_t)3) - 1 ? "" : ", ");
            }
            if (vec.size() > 3) cout << "...";
            cout << "]" << endl;
        }

        // 维度异常检测（仅针对完整记录）
        if (vec.size() != (size_t)global_dim) {
            cout << ">> [警告] 发现维度不一致！第 " << count << " 条数据维度为 " << vec.size() << endl;
        }
    }

    cout << "---------------------------" << endl;
    cout << "检测完成！" << endl;
    cout << "总条数: " << count << endl;
    cout << "统一维度: " << global_dim << endl;
    
    ifs.close();
    return 0;
}