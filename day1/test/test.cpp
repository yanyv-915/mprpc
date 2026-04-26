#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

enum class OpCode:uint8_t{
    SET = 1,
    GET = 2,
    DEL = 3,
    SEARCH = 4,
    UNKNOWN = 0,
};

enum class DataType:uint8_t{
    FLOAT32 = 1,
    INT16 = 2,
    BINARY = 3,
    UNKONWN = 0,
};

#pragma pack(push,1)
struct MessageHeader{
    uint16_t magic; // 0x4647
    OpCode op;     // 1:SET, 2:GET, 3:DEL, 4:SEARCH
    DataType  dataType;// <-- 新增：1:FLOAT32, 2:INT8, 3:BINARY...
    uint64_t key_id;
    uint32_t dim;
};
#pragma pack(pop)

class VectorClient {
private:
    int sock = -1;
    const char* server_ip;
    int server_port;

public:
    VectorClient(const char* ip, int port) : server_ip(ip), server_port(port) {}

    bool connectServer() {
        if (sock != -1) close(sock);
        sock = socket(AF_INET, SOCK_STREAM, 0);
        
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(server_port);
        inet_pton(AF_INET, server_ip, &addr.sin_addr);

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("Connect failed");
            return false;
        }
        return true;
    }

    // 解决粘包的核心：必须读够 len 字节
    bool recvAll(void* buf, size_t len) {
        size_t total = 0;
        char* p = static_cast<char*>(buf);
        while (total < len) {
            ssize_t n = recv(sock, p + total, len - total, 0);
            if (n <= 0) return false; // 连接断开或出错
            total += n;
        }
        return true;
    }

    bool setVector(uint64_t id, uint32_t dim, const std::vector<float>& vec) {
        MessageHeader header;
        header.magic = 0x4647;
        header.op = OpCode::SET; // SET
        header.dataType = DataType::FLOAT32; // FLOAT32
        header.key_id = id;
        header.dim = dim;

        // 发送 Header
        if (send(sock, &header, sizeof(header), 0) < 0) return false;
        // 发送 Body
        if (send(sock, vec.data(), dim * sizeof(float), 0) < 0) return false;

        // 等待响应 Header
        MessageHeader res;
        if (!recvAll(&res, sizeof(res))) {
            std::cout << "[!] Connection lost at ID: " << id << std::endl;
            return false;
        }
        return res.key_id == 1; // 假设服务端返回1表示成功
    }

    void closeConn() {
        if (sock != -1) close(sock);
        sock = -1;
    }
};

int main() {
    VectorClient client("127.0.0.1", 8080);
    if (!client.connectServer()) return -1;

    uint32_t dim = 4;
    int test_count = 20;
    int success_count = 0;

    std::cout << "--- 开始 C++ 批量插入测试 ---" << std::endl;

    for (int i = 0; i < test_count; ++i) {
        std::vector<float> vec = { (float)i, (float)i+1, (float)i+2, (float)i+3 };
        
        if (client.setVector(i, dim, vec)) {
            std::cout << "Successfully inserted ID: " << i << std::endl;
            success_count++;
        } else {
            std::cout << "Failed at ID: " << i << ". Attempting to reconnect..." << std::endl;
            if (!client.connectServer()) break; 
        }
        // usleep(1000); // 如果服务端处理太慢，可以稍微延迟
    }

    std::cout << "\n测试完成! 成功: " << success_count << "/" << test_count << std::endl;
    return 0;
}