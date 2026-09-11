#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

// ======================== 跨平台 Socket 兼容层 ========================
#ifdef _WIN32
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
using socklen_t = int;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using SOCKET = int;
#define INVALID_SOCKET (SOCKET)(~0)
#define SOCKET_ERROR (-1)
#define closesocket(s) close(s)
#endif

enum GamePhase {
    GAME_OVER = -1,
    JOINING = 0,
    NIGHT_START,
    PHASE_DOPPEL,
    PHASE_WEREWOLF,
    PHASE_MINION,
    PHASE_SEER,
    PHASE_ROBBER,
    PHASE_TROUBLEMAKER,
    PHASE_DRUNK,
    PHASE_INSOMNIAC,
    PHASE_REVEALER,
    DAY_DISCUSSION
};

struct Player {
    SOCKET sock = INVALID_SOCKET;
    int player_id = -1;
    bool ready = false;
    std::string name;
    std::string initial_role;
    std::string current_role;

    std::string doppel_copy_role;
    bool doppel_skill_used = false;
    bool is_revealed = false;
    bool drunk_took_card = false;
    bool has_acted = false;
    bool has_voted = false;
    int voted_target = -1;
};

// 一个 Room 对应一份 GameState。游戏规则函数只操作传入房间的状态，
// 不再依赖进程级全局变量，因此多个房间可以并发运行。
struct GameState {
    GamePhase current_phase = JOINING;
    std::vector<Player> players;
    bool is_game_started = false;
    bool voting_open = false;

    const std::vector<std::string> base_cards = {
        "狼人1", "狼人2", "爪牙", "捣蛋鬼", "强盗", "预言家", "酒鬼"
    };
    const std::vector<std::string> extra_pool = {
        "失眠者", "幽灵", "皮匠", "揭示者", "平民"
    };
    std::vector<std::string> current_deck;
    std::vector<std::string> table_cards;
    std::map<int, bool> table_cards_taken;
};
