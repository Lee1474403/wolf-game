#include "communication.h"
#include "network.h"
#include "room_manager.h"
#include "server_config.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <memory>
#include <streambuf>
#include <thread>

namespace {

class TeeBuffer : public std::streambuf {
public:
    TeeBuffer(std::streambuf* first, std::streambuf* second)
        : m_first(first), m_second(second) {}

protected:
    int overflow(int ch) override {
        if (ch == traits_type::eof()) return traits_type::not_eof(ch);
        const bool firstOk = m_first->sputc(static_cast<char>(ch)) != traits_type::eof();
        const bool secondOk = m_second->sputc(static_cast<char>(ch)) != traits_type::eof();
        return firstOk && secondOk ? ch : traits_type::eof();
    }

    int sync() override {
        return m_first->pubsync() == 0 && m_second->pubsync() == 0 ? 0 : -1;
    }

private:
    std::streambuf* m_first;
    std::streambuf* m_second;
};

void configureClientSocket(SOCKET socket) {
    int keepAlive = 1;
#ifdef _WIN32
    const DWORD sendTimeoutMs = 5000;
    setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE,
               reinterpret_cast<const char*>(&keepAlive), sizeof(keepAlive));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO,
               reinterpret_cast<const char*>(&sendTimeoutMs), sizeof(sendTimeoutMs));
#else
    const timeval sendTimeout{5, 0};
    setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &sendTimeout, sizeof(sendTimeout));
#endif
}

} // namespace

int main(int argc, char* argv[]) {
    ServerConfig config;
    try {
        config = ServerConfig::fromEnvironmentAndArgs(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "configuration error: " << error.what() << '\n';
        return 2;
    }

    std::ofstream logFile;
    std::unique_ptr<TeeBuffer> coutTee;
    std::unique_ptr<TeeBuffer> cerrTee;
    std::streambuf* originalCout = std::cout.rdbuf();
    std::streambuf* originalCerr = std::cerr.rdbuf();
    if (!config.log_file.empty()) {
        logFile.open(config.log_file, std::ios::app);
        if (logFile) {
            coutTee = std::make_unique<TeeBuffer>(originalCout, logFile.rdbuf());
            cerrTee = std::make_unique<TeeBuffer>(originalCerr, logFile.rdbuf());
            std::cout.rdbuf(coutTee.get());
            std::cerr.rdbuf(cerrTee.get());
            std::cout.setf(std::ios::unitbuf);
            std::cerr.setf(std::ios::unitbuf);
        } else {
            std::cerr << "warning: unable to open log file " << config.log_file << '\n';
        }
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Winsock startup failed\n";
        return 1;
    }
#else
    std::signal(SIGPIPE, SIG_IGN);
#endif

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "failed to create listening socket\n";
        return 1;
    }

    int reuse = 1;
#ifdef _WIN32
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#else
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(config.port);
    address.sin_addr.s_addr = INADDR_ANY;
    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) ==
        SOCKET_ERROR) {
        std::cerr << "bind failed on port " << config.port << '\n';
        closesocket(listenSocket);
        return 1;
    }
    if (listen(listenSocket, config.backlog) == SOCKET_ERROR) {
        std::cerr << "listen failed\n";
        closesocket(listenSocket);
        return 1;
    }

    RoomManager roomManager(config.max_rooms, config.phase_seconds,
                            config.discussion_seconds, config.ready_timeout_seconds);
    std::atomic<std::size_t> activeConnections{0};
    std::thread([&roomManager] {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            roomManager.expireInactiveRooms();
        }
    }).detach();

    std::cout << "werewolf server listening on 0.0.0.0:" << config.port
              << ", max connections=" << config.max_connections
              << ", max rooms=" << (config.max_rooms == 0 ? "unlimited" :
                                     std::to_string(config.max_rooms)) << '\n';

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientSize = sizeof(clientAddress);
        SOCKET client = accept(listenSocket, reinterpret_cast<sockaddr*>(&clientAddress),
                               &clientSize);
        if (client == INVALID_SOCKET) continue;
        configureClientSocket(client);

        if (activeConnections.load() >= config.max_connections) {
            sendLine(client, "JOIN_REJECTED|SERVER_BUSY|服务器连接数已满，请稍后重试");
            closesocket(client);
            continue;
        }

        ++activeConnections;
        std::thread([client, &roomManager, &activeConnections] {
            handle_client(client, roomManager);
            --activeConnections;
        }).detach();
    }

    closesocket(listenSocket);
#ifdef _WIN32
    WSACleanup();
#endif
    std::cout.rdbuf(originalCout);
    std::cerr.rdbuf(originalCerr);
    return 0;
}
