#include "communication.h"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <vector>

namespace {

bool sendAll(SOCKET socket, const std::string& payload) {
    std::size_t sent = 0;
    while (sent < payload.size()) {
        const int result = send(socket, payload.data() + sent,
                                static_cast<int>(payload.size() - sent), 0);
        if (result <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(result);
    }
    return true;
}

std::string withNewline(const std::string& message) {
    if (!message.empty() && message.back() == '\n') {
        return message;
    }
    return message + "\n";
}

} // namespace

bool sendLine(SOCKET socket, const std::string& message) {
    return sendAll(socket, withNewline(message));
}

bool sendLine(Room& room, SOCKET socket, const std::string& message) {
    std::lock_guard<std::mutex> sendLock(room.send_mutex);
    return sendAll(socket, withNewline(message));
}

void broadcast(Room& room, const std::string& message) {
    std::vector<SOCKET> targets;
    {
        std::lock_guard<std::mutex> lock(room.mutex);
        for (const Player& player : room.game.players) {
            targets.push_back(player.sock);
        }
    }
    for (SOCKET socket : targets) {
        sendLine(room, socket, message);
    }
}

void broadcastRoomStatus(Room& room) {
    std::string status;
    std::vector<SOCKET> targets;
    {
        std::lock_guard<std::mutex> lock(room.mutex);
        std::ostringstream out;
        out << "ROOM_STATUS|" << room.code
            << '|' << room.game.players.size()
            << '|' << kRoomCapacity
            << '|' << (room.game.is_game_started ? 1 : 0)
            << '|' << room.hostPlayerIdUnlocked();
        for (const Player& player : room.game.players) {
            out << '|' << player.player_id << ',' << escapeProtocolField(player.name)
                << ',' << (player.ready ? 1 : 0);
            targets.push_back(player.sock);
        }
        status = out.str();
    }
    for (SOCKET socket : targets) {
        sendLine(room, socket, status);
    }
}

void send_to_role(Room& room, const std::string& roleTarget, const std::string& message) {
    std::vector<SOCKET> targets;
    {
        std::lock_guard<std::mutex> lock(room.mutex);
        for (const Player& player : room.game.players) {
            if (player.initial_role == roleTarget) {
                targets.push_back(player.sock);
            }
        }
    }
    for (SOCKET socket : targets) {
        sendLine(room, socket, message);
    }
}

void send_to_doppel_role(Room& room, const std::string& roleTarget,
                         const std::string& message) {
    std::vector<SOCKET> targets;
    {
        std::lock_guard<std::mutex> lock(room.mutex);
        for (const Player& player : room.game.players) {
            if (player.initial_role == roleTarget || player.doppel_copy_role == roleTarget) {
                targets.push_back(player.sock);
            }
        }
    }
    for (SOCKET socket : targets) {
        sendLine(room, socket, message);
    }
}

void broadcastPhase(Room& room, const std::string& phase, const std::string& step,
                    const std::string& prompt) {
    broadcast(room, "PHASE|" + escapeProtocolField(phase) + "|" +
                        escapeProtocolField(step) + "|" + escapeProtocolField(prompt));
}

void sendAction(Room& room, SOCKET socket, const std::string& action,
                const std::string& prompt) {
    sendLine(room, socket, "ACTION|" + escapeProtocolField(action) + "|" +
                               escapeProtocolField(prompt));
}

std::string escapeProtocolField(const std::string& value) {
    std::ostringstream escaped;
    escaped << std::uppercase << std::hex;
    for (const unsigned char ch : value) {
        if (ch == '%' || ch == '|' || ch == ',' || ch == ';' || ch == '\n' || ch == '\r') {
            escaped << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        } else {
            escaped << static_cast<char>(ch);
        }
    }
    return escaped.str();
}

std::string unescapeProtocolField(const std::string& value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size() &&
            std::isxdigit(static_cast<unsigned char>(value[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(value[i + 2]))) {
            const std::string hex = value.substr(i + 1, 2);
            decoded.push_back(static_cast<char>(std::stoi(hex, nullptr, 16)));
            i += 2;
        } else {
            decoded.push_back(value[i]);
        }
    }
    return decoded;
}

std::vector<std::string> splitProtocol(const std::string& value, char delimiter) {
    std::vector<std::string> fields;
    std::stringstream stream(value);
    std::string field;
    while (std::getline(stream, field, delimiter)) {
        fields.push_back(field);
    }
    if (!value.empty() && value.back() == delimiter) {
        fields.emplace_back();
    }
    return fields;
}
