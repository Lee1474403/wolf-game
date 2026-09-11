#pragma once

#include "room.h"

#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

enum class JoinStatus {
    Joined,
    InvalidCode,
    InvalidName,
    RoomFull,
    GameInProgress,
    RoomLimitReached
};

struct JoinResult {
    JoinStatus status = JoinStatus::InvalidCode;
    std::shared_ptr<Room> room;
    int player_id = -1;
    bool created = false;
    bool is_host = false;
    std::string message;
};

class RoomManager {
public:
    RoomManager(std::size_t maxRooms, int phaseSeconds, int discussionSeconds,
                int readyTimeoutSeconds);

    JoinResult join(const std::string& code, const std::string& nickname, SOCKET sock);
    void leave(const std::shared_ptr<Room>& room, SOCKET sock);
    void dissolve(const std::shared_ptr<Room>& room, const std::string& reason,
                  SOCKET disconnectedSocket = INVALID_SOCKET);
    void expireInactiveRooms();
    std::size_t roomCount() const;

private:
    const std::size_t m_maxRooms;
    const int m_phaseSeconds;
    const int m_discussionSeconds;
    const int m_readyTimeoutSeconds;
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<Room>> m_rooms;

    void eraseIfSameRoom(const std::string& code, const std::shared_ptr<Room>& room);
};
