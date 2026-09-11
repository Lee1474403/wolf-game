#include "network.h"

#include "communication.h"
#include "game_flow.h"
#include "game_rules.h"
#include "night_action.h"
#include "voting.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(),
                                        [](unsigned char ch) { return std::isspace(ch) != 0; });
    const auto last = std::find_if_not(value.rbegin(), value.rend(),
                                       [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    if (first >= last) return {};
    return std::string(first, last);
}

void sendError(Room& room, SOCKET socket, const std::string& code,
               const std::string& message) {
    sendLine(room, socket, "ERROR|" + escapeProtocolField(code) + "|" +
                               escapeProtocolField(message));
}

std::string joinStatusCode(JoinStatus status) {
    switch (status) {
    case JoinStatus::RoomFull: return "ROOM_FULL";
    case JoinStatus::GameInProgress: return "GAME_IN_PROGRESS";
    case JoinStatus::RoomLimitReached: return "ROOM_LIMIT";
    case JoinStatus::InvalidName: return "INVALID_NAME";
    case JoinStatus::InvalidCode: return "INVALID_CODE";
    case JoinStatus::Joined: return "JOINED";
    }
    return "JOIN_FAILED";
}

bool startGame(const std::shared_ptr<Room>& room, SOCKET requester) {
    std::vector<std::pair<SOCKET, std::string>> roles;
    std::uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(room->mutex);
        if (room->dissolved) return false;
        if (!room->isHostUnlocked(requester)) {
            sendError(*room, requester, "HOST_ONLY", "只有房主可以开始游戏");
            return false;
        }
        if (room->game.is_game_started) {
            sendError(*room, requester, "ALREADY_STARTED", "游戏已经开始");
            return false;
        }
        if (room->game.players.size() < kMinPlayers) {
            sendError(*room, requester, "NOT_ENOUGH_PLAYERS", "至少需要 7 名玩家才能开始");
            return false;
        }
        if (!room->allPlayersReadyUnlocked()) {
            sendError(*room, requester, "NOT_ALL_READY", "仍有玩家未准备");
            return false;
        }

        prepare_deck(*room, static_cast<int>(room->game.players.size()));
        std::shuffle(room->game.current_deck.begin(), room->game.current_deck.end(),
                     std::mt19937(std::random_device{}()));
        room->game.table_cards.clear();
        for (std::size_t i = 0; i < room->game.players.size(); ++i) {
            Player& player = room->game.players[i];
            player.initial_role = room->game.current_deck[i];
            player.current_role = room->game.current_deck[i];
            roles.emplace_back(player.sock, player.initial_role);
        }
        for (std::size_t i = room->game.players.size();
             i < room->game.current_deck.size(); ++i) {
            room->game.table_cards.push_back(room->game.current_deck[i]);
        }
        room->game.is_game_started = true;
        room->game.current_phase = NIGHT_START;
        room->game.voting_open = false;
        generation = ++room->generation;
        room->touchUnlocked();
    }

    for (const auto& [socket, role] : roles) {
        sendLine(*room, socket, "ROLE|" + escapeProtocolField(role));
    }
    broadcast(*room, "GAME_START|" + room->code);
    broadcastRoomStatus(*room);
    std::cout << "[room " << room->code << "] game started, generation "
              << generation << '\n';
    std::thread(game_flow_controller, room).detach();
    return true;
}

void updateReady(const std::shared_ptr<Room>& room, SOCKET socket, bool ready) {
    {
        std::lock_guard<std::mutex> lock(room->mutex);
        if (room->game.is_game_started) {
            sendError(*room, socket, "GAME_STARTED", "游戏开始后不能修改准备状态");
            return;
        }
        Player* player = room->findPlayerUnlocked(socket);
        if (!player) return;
        player->ready = ready;
        room->touchUnlocked();
    }
    sendLine(*room, socket,
             std::string("READY_ACK|") + (ready ? "1|已准备" : "0|已取消准备"));
    broadcastRoomStatus(*room);
}

int commandTarget(const std::string& command) {
    const std::size_t separator = command.find_first_of(" |\t");
    if (separator == std::string::npos) return -1;
    try {
        return std::stoi(trim(command.substr(separator + 1)));
    } catch (...) {
        return -1;
    }
}

void handleVote(const std::shared_ptr<Room>& room, SOCKET socket,
                const std::string& command) {
    bool allVoted = false;
    int targetId = commandTarget(command);
    {
        std::lock_guard<std::mutex> lock(room->mutex);
        if (room->game.current_phase != DAY_DISCUSSION || !room->game.voting_open) {
            sendError(*room, socket, "VOTE_NOT_OPEN", "当前还未进入投票阶段");
            return;
        }
        Player* player = room->findPlayerUnlocked(socket);
        if (!player) return;
        if (player->has_voted) {
            sendError(*room, socket, "ALREADY_VOTED", "你已经提交过选票");
            return;
        }
        if (targetId < 1 || targetId > static_cast<int>(room->game.players.size())) {
            sendError(*room, socket, "INVALID_TARGET", "请选择有效的玩家席位");
            return;
        }

        player->voted_target = targetId - 1;
        player->has_voted = true;
        allVoted = std::all_of(room->game.players.begin(), room->game.players.end(),
                               [](const Player& current) { return current.has_voted; });
        if (allVoted) {
            room->game.current_phase = GAME_OVER;
            room->game.voting_open = false;
        }
    }

    sendLine(*room, socket, "NOTICE|投票已提交|你的选票已记录，请等待其他玩家");
    if (allVoted) {
        broadcast(*room, "PHASE|ENDING|RESULT|所有玩家已投票，正在结算");
        std::thread([room] { announce_vote_result(*room); }).detach();
    }
}

void processCommand(const std::shared_ptr<Room>& room, SOCKET socket,
                    const std::string& command) {
    if (command == "READY" || command == "READY|1") {
        updateReady(room, socket, true);
        return;
    }
    if (command == "UNREADY" || command == "READY|0") {
        updateReady(room, socket, false);
        return;
    }
    if (command == "START") {
        startGame(room, socket);
        return;
    }
    if (command.rfind("VOTE", 0) == 0) {
        handleVote(room, socket, command);
        return;
    }

    GamePhase phase;
    {
        std::lock_guard<std::mutex> lock(room->mutex);
        phase = room->game.current_phase;
    }
    if (phase >= PHASE_DOPPEL && phase <= PHASE_REVEALER) {
        process_night_action(*room, socket, command);
    } else {
        sendError(*room, socket, "INVALID_COMMAND", "当前阶段不能执行该操作");
    }
}

} // namespace

void handle_client(SOCKET clientSocket, RoomManager& roomManager) {
    std::shared_ptr<Room> room;
    std::string pending;
    char buffer[2048];

    while (true) {
        const int bytes = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;
        pending.append(buffer, static_cast<std::size_t>(bytes));
        if (pending.size() > 16384) {
            if (room) {
                sendError(*room, clientSocket, "MESSAGE_TOO_LARGE", "单条消息过长");
            } else {
                sendLine(clientSocket, "JOIN_REJECTED|MESSAGE_TOO_LARGE|单条消息过长");
            }
            break;
        }

        std::size_t newline = 0;
        while ((newline = pending.find('\n')) != std::string::npos) {
            std::string command = trim(pending.substr(0, newline));
            pending.erase(0, newline + 1);
            if (command.empty()) continue;

            if (!room) {
                const std::vector<std::string> fields = splitProtocol(command);
                if (fields.size() < 3 || fields[0] != "JOIN") {
                    sendLine(clientSocket,
                             "JOIN_REJECTED|PROTOCOL|请先发送 JOIN 加入四位房间码");
                    continue;
                }
                JoinResult result = roomManager.join(fields[1],
                                                     trim(unescapeProtocolField(fields[2])),
                                                     clientSocket);
                if (result.status != JoinStatus::Joined) {
                    sendLine(clientSocket, "JOIN_REJECTED|" + joinStatusCode(result.status) +
                                               "|" + escapeProtocolField(result.message));
                    continue;
                }
                room = result.room;
                sendLine(*room, clientSocket,
                         "JOINED|" + room->code + '|' + std::to_string(result.player_id) +
                             '|' + (result.is_host ? "1" : "0") +
                             '|' + (result.created ? "1" : "0"));
                sendLine(*room, clientSocket,
                         "NOTICE|欢迎加入|" + escapeProtocolField(result.message));
                broadcastRoomStatus(*room);
            } else {
                processCommand(room, clientSocket, command);
            }
        }
    }

    if (room) {
        roomManager.leave(room, clientSocket);
    }
    closesocket(clientSocket);
}
