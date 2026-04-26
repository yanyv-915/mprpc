#include "../include/network/TcpServer.h"
#include "../include/core/config.h"
int main(){
    Config::getInstance().load("../config/config.yaml");

    // 2. 使用加载好的参数启动服务器
    int server_port = Config::getInstance().port;
    int threads = Config::getInstance().thread_pool_size;
    (void)threads;
    std::cout << "服务器尝试在端口 " << server_port << " 启动..." << std::endl;
    
    // TcpServer server(server_port, threads);
    // server.start();

    return 0;
    // Tcp tcp;
    // tcp.run();
    // std::cout << "[Main] 程序正常退出。" << std::endl;
}