#include "../include/network/TcpServer.h"
int main(){
    Tcp tcp;
    tcp.run();
    std::cout << "[Main] 程序正常退出。" << std::endl;
}