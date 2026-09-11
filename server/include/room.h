#pragma once

#include "game_types.h"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

constexpr int kMinPlayers = 7;
constexpr int kRoomCapacity = 9;

class Room : public std::enable_shared_from_this<Room> {
public:
    Room(std::string roomCode, int phaseSeconds, int discussionSeconds);

    const std::string code;
    const int phase_seconds;
    const int discussion_seconds;

    mutable std::mutex mutex;
    std::mutex send_mutex;
    std::condition_variable state_changed;
    GameState game;
    SOCKET host_socket = INVALID_SOCKET;
    std::uint64_t generation = 0;
    bool dissolved = false;
    std::chrono::steady_clock::time_point last_activity;

    Player* findPlayerUnlocked(SOCKET sock);
    const Player* findPlayerUnlocked(SOCKET sock) const;
    bool isHostUnlocked(SOCKET sock) const;
    int hostPlayerIdUnlocked() const;
    bool allPlayersReadyUnlocked() const;
    void touchUnlocked();
    void renumberPlayersUnlocked();
    void resetForNextRoundUnlocked();
};
