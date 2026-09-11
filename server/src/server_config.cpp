#include "server_config.h"

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

std::string environmentValue(const char* name) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

long long parseInteger(const std::string& value, const std::string& option,
                       long long minimum, long long maximum) {
    if (value.empty()) {
        throw std::runtime_error(option + " cannot be empty");
    }
    std::size_t consumed = 0;
    const long long parsed = std::stoll(value, &consumed);
    if (consumed != value.size() || parsed < minimum || parsed > maximum) {
        throw std::runtime_error(option + " is outside the allowed range");
    }
    return parsed;
}

std::string argumentValue(const std::string& argument, const std::string& name) {
    const std::string prefix = "--" + name + '=';
    return argument.rfind(prefix, 0) == 0 ? argument.substr(prefix.size()) : std::string();
}

template <typename T>
void applyNumeric(T& target, const std::string& value, const std::string& name,
                  long long minimum, long long maximum) {
    if (!value.empty()) {
        target = static_cast<T>(parseInteger(value, name, minimum, maximum));
    }
}

} // namespace

ServerConfig ServerConfig::fromEnvironmentAndArgs(int argc, char* argv[]) {
    ServerConfig config;
    applyNumeric(config.port, environmentValue("GAME_PORT"), "GAME_PORT", 1, 65535);
    applyNumeric(config.backlog, environmentValue("GAME_BACKLOG"), "GAME_BACKLOG", 1, 65535);
    applyNumeric(config.max_connections, environmentValue("GAME_MAX_CONNECTIONS"),
                 "GAME_MAX_CONNECTIONS", 9, 1000000);
    applyNumeric(config.max_rooms, environmentValue("GAME_MAX_ROOMS"),
                 "GAME_MAX_ROOMS", 0, 1000000);
    applyNumeric(config.phase_seconds, environmentValue("GAME_PHASE_SECONDS"),
                 "GAME_PHASE_SECONDS", 1, 3600);
    applyNumeric(config.discussion_seconds, environmentValue("GAME_DISCUSSION_SECONDS"),
                 "GAME_DISCUSSION_SECONDS", 0, 3600);
    applyNumeric(config.ready_timeout_seconds,
                 environmentValue("GAME_READY_TIMEOUT_SECONDS"),
                 "GAME_READY_TIMEOUT_SECONDS", 0, 86400);
    config.log_file = environmentValue("GAME_LOG_FILE");

    for (int i = 1; i < argc; ++i) {
        const std::string argument(argv[i]);
        std::string value;
        if (!(value = argumentValue(argument, "port")).empty()) {
            applyNumeric(config.port, value, "--port", 1, 65535);
        } else if (!(value = argumentValue(argument, "backlog")).empty()) {
            applyNumeric(config.backlog, value, "--backlog", 1, 65535);
        } else if (!(value = argumentValue(argument, "max-connections")).empty()) {
            applyNumeric(config.max_connections, value, "--max-connections", 9, 1000000);
        } else if (!(value = argumentValue(argument, "max-rooms")).empty()) {
            applyNumeric(config.max_rooms, value, "--max-rooms", 0, 1000000);
        } else if (!(value = argumentValue(argument, "phase-seconds")).empty()) {
            applyNumeric(config.phase_seconds, value, "--phase-seconds", 1, 3600);
        } else if (!(value = argumentValue(argument, "discussion-seconds")).empty()) {
            applyNumeric(config.discussion_seconds, value, "--discussion-seconds", 0, 3600);
        } else if (!(value = argumentValue(argument, "ready-timeout-seconds")).empty()) {
            applyNumeric(config.ready_timeout_seconds, value,
                         "--ready-timeout-seconds", 0, 86400);
        } else if (!(value = argumentValue(argument, "log-file")).empty()) {
            config.log_file = value;
        } else {
            throw std::runtime_error("unknown argument: " + argument);
        }
    }
    return config;
}
