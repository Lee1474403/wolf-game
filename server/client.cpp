#include <iostream>
#include <string>
#include <thread>
#include <cstring>

#ifdef _WIN32
    #define _WINSOCK_DEPRECATED_NO_WARNINGS
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define closesocket close
    typedef int SOCKET;
    const int INVALID_SOCKET = -1;
    const int SOCKET_ERROR = -1;
#endif

int main() {
    // 1. 解决 Windows 控制台中文乱码问题
    // 如果你服务端发送的是 UTF-8 编码，这里将控制台设为 UTF-8 (65001)
#ifdef _WIN32
    system("chcp 65001 > nul"); 
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "创建 Socket 失败" << std::endl;
        return 1;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    // 如果是连远程服务器，把 127.0.0.1 改为服务器的公网 IP
    addr.sin_addr.s_addr = inet_addr("172.28.113.66");

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        std::cout << "连接服务器失败！请检查服务端是否开启。" << std::endl;
        return 1;
    }

    std::cout << "------------------------------------------" << std::endl;
    std::cout << "已进入狼人杀房间。输入 READY 并回车进行准备" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    
    // 2. 接收线程：负责监听服务端发来的身份和系统消息
    std::thread recvThread([sock]() {
        char buf[1024];
        while (true) {
            memset(buf, 0, sizeof(buf));
            int len = recv(sock, buf, sizeof(buf) - 1, 0);
            if (len > 0) {
                // 简单换行
                std::cout << "\n" ;
                std::cout << "[系统消息] " << buf << std::endl;
                std::cout << ">>> "; // 使用更简洁的提示符
                std::cout.flush();
            } else if (len == 0) {
                std::cout << "\n服务器已关闭连接。" << std::endl;
                break;
            } else {
                std::cerr << "\n接收数据错误。" << std::endl;
                break;
            }
        }
    });
    recvThread.detach();

    // 3. 主线程：处理用户输入并发送
    std::string input;
    while (true) {
        // 使用 getline 以支持带空格的输入（虽然 READY 不需要，但后续技能可能需要）
        if (!std::getline(std::cin, input)) break;
        if (input.empty()) continue;
        if (input == "QUIT" || input == "exit") {
            std::cout << "正在退出游戏..." << std::endl;
            break; 
        }
        int sendLen = send(sock, input.c_str(), (int)input.length(), 0);
        if (sendLen == SOCKET_ERROR) {
            std::cerr << "发送失败。" << std::endl;
            break;
        }

        
    }

    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}