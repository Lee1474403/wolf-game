#include "room_manager.h"

#include "communication.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iostream>
#include <utility>
#include <vector>

namespace {

bool isValidRoomCode(const std::string& code) {
    return code.size() == 4 &&
           std::all_of(code.begin(), code.end(),
                       [](unsigned char ch) { return std::isdigit(ch) != 0; });
}

void shutdownSocket(SOCKET socket) {
    if (socket == INVALID_SOCKET) {
        return;
    }
#ifdef _WIN32
    shutdown(socket, SD_BOTH);
#else
    shutdown(socket, SHUT_RDWR);
#endif
}

} // namespace

RoomManager::RoomManager(std::size_t maxRooms, int phaseSeconds, int discussionSeconds,
                         int readyTimeoutSeconds)
    : m_maxRooms(maxRooms),
      m_phaseSeconds(phaseSeconds),
      m_discussionSeconds(discussionSeconds),
      m_readyTimeoutSeconds(readyTimeoutSeconds) {}

JoinResult RoomManager::join(const std::string& code, const std::string& nickname,
                             SOCKET sock) {
    if (!isValidRoomCode(code)) {
        return {JoinStatus::InvalidCode, nullptr, -1, false, false,
                "房间码必须是四位数字"};
    }
    if (nickname.empty() || nickname.size() > 60) {
        return {JoinStatus::InvalidName, nullptr, -1, false, false,
                "昵称不能为空且不能超过 20 个中文字符"};
    }

    std::lock_guard<std::mutex> managerLock(m_mutex);
    auto it = m_rooms.find(code);
    bool created = false;
    std::shared_ptr<Room> room;

    if (it == m_rooms.end()) {
        if (m_maxRooms > 0 && m_rooms.size() >= m_maxRooms) {
            return {JoinStatus::RoomLimitReached, nullptr, -1, false, false,
                    "服务器房间数量已达上限，请稍后重试"};
        }
        room = std::make_shared<Room>(code, m_phaseSeconds, m_discussionSeconds);
        m_rooms.emplace(code, room);
        created = true;
    } else {
        room = it->second;
    }

    std::lock_guard<std::mutex> roomLock(room->mutex);
    if (room->dissolved || room->game.is_game_started) {
        if (created) {
            m_rooms.erase(code);
        }
        return {JoinStatus::GameInProgress, nullptr, -1, false, false,
                "该房间游戏正在进行，请使用其他房间码"};
    }
    if (room->game.players.size() >= kRoomCapacity) {
        return {JoinStatus::RoomFull, nullptr, -1, false, false,
                "该房间已满（9/9）"};
    }

    Player player;
    player.sock = sock;
    player.player_id = static_cast<int>(room->game.players.size()) + 1;
    player.name = nickname;
    room->game.players.push_back(player);
    if (room->host_socket == INVALID_SOCKET) {
        room->host_socket = sock;
    }
    room->touchUnlocked();

    std::cout << "[room " << code << "] player joined: " << nickname
              << " (" << room->game.players.size() << '/' << kRoomCapacity << ")\n";
    return {JoinStatus::Joined, room, player.player_id, created,
            room->isHostUnlocked(sock), created ? "房间已创建" : "已加入房间"};
}

void RoomManager::leave(const std::shared_ptr<Room>& room, SOCKET sock) {
    if (!room) {
        return;
    }

    bool dissolveRunningGame = false;
    bool roomBecameEmpty = false;
    bool hostChanged = false;
    std::string playerName;
    std::vector<std::pair<SOCKET, int>> reassignedIds;
    {
        std::lock_guard<std::mutex> lock(room->mutex);
        if (room->dissolved) {
            return;
        }
        auto it = std::find_if(room->game.players.begin(), room->game.players.end(),
                               [sock](const Player& player) { return player.sock == sock; });
        if (it == room->game.players.end()) {
            return;
        }

        playerName = it->name;
        if (room->game.is_game_started) {
            dissolveRunningGame = true;
        } else {
            const bool wasHost = room->host_socket == sock;
            room->game.players.erase(it);
            room->renumberPlayersUnlocked();
            for (const Player& player : room->game.players) {
                reassignedIds.emplace_back(player.sock, player.player_id);
            }
            if (wasHost) {
                room->host_socket = room->game.players.empty()
                                        ? INVALID_SOCKET
                                        : room->game.players.front().sock;
                hostChanged = !room->game.players.empty();
            }
            room->touchUnlocked();
            roomBecameEmpty = room->game.players.empty();
        }
    }

    if (dissolveRunningGame) {
        dissolve(room, "有玩家在游戏中掉线，本房间已安全解散", sock);
        return;
    }

    std::cout << "[room " << room->code << "] player left: " << playerName << '\n';
    if (roomBecameEmpty) {
        eraseIfSameRoom(room->code, room);
        return;
    }
    if (hostChanged) {
        broadcast(*room, "NOTICE|新房主|原房主已离开，房主已自动移交给最早加入的玩家");
    }
    for (const auto& [socket, playerId] : reassignedIds) {
        sendLine(*room, socket, "PLAYER_ID|" + std::to_string(playerId));
    }
    broadcastRoomStatus(*room);
}

void RoomManager::dissolve(const std::shared_ptr<Room>& room, const std::string& reason,
                           SOCKET disconnectedSocket) {
    if (!room) {
        return;
    }

    std::vector<SOCKET> sockets;
    {
        std::lock_guard<std::mutex> lock(room->mutex);
        if (room->dissolved) {
            return;
        }
        room->dissolved = true;
        ++room->generation;
        for (const Player& player : room->game.players) {
            if (player.sock != disconnectedSocket) {
                sockets.push_back(player.sock);
            }
        }
        room->game.players.clear();
        room->game.is_game_started = false;
        room->game.current_phase = GAME_OVER;
    }
    room->state_changed.notify_all();
    eraseIfSameRoom(room->code, room);

    const std::string message = "ROOM_DISBANDED|" + escapeProtocolField(reason);
    for (SOCKET socket : sockets) {
        sendLine(*room, socket, message);
        shutdownSocket(socket);
    }
    std::cout << "[room " << room->code << "] dissolved: " << reason << '\n';
}

void RoomManager::expireInactiveRooms() {
    if (m_readyTimeoutSeconds <= 0) {
        return;
    }

    std::vector<std::shared_ptr<Room>> rooms;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& [code, room] : m_rooms) {
            (void)code;
            rooms.push_back(room);
        }
    }

    const auto now = std::chrono::steady_clock::now();
    for (const auto& room : rooms) {
        bool expired = false;
        {
            std::lock_guard<std::mutex> lock(room->mutex);
            expired = !room->dissolved && !room->game.is_game_started &&
                      !room->game.players.empty() &&
                      std::chrono::duration_cast<std::chrono::seconds>(
                          now - room->last_activity).count() >= m_readyTimeoutSeconds;
        }
        if (expired) {
            dissolve(room, "房间长时间无人准备，已自动解散，请重新加入");
        }
    }
}

std::size_t RoomManager::roomCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rooms.size();
}

void RoomManager::eraseIfSameRoom(const std::string& code,
                                  const std::shared_ptr<Room>& room) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_rooms.find(code);
    if (it != m_rooms.end() && it->second == room) {
        m_rooms.erase(it);
    }
}
