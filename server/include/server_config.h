#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

struct ServerConfig {
    std::uint16_t port = 8888;
    int backlog = 128;
    std::size_t max_connections = 1024;
    std::size_t max_rooms = 0;
    int phase_seconds = 60;
    int discussion_seconds = 60;
    int ready_timeout_seconds = 300;
    std::string log_file;

    static ServerConfig fromEnvironmentAndArgs(int argc, char* argv[]);
};
