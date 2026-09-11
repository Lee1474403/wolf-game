#include "voting.h"

#include "communication.h"
#include "game_flow.h"
#include "game_rules.h"
#include "room.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

void announce_vote_result(Room& room) {
    std::string winner;
    std::string identities;
    std::string consoleResult;
    std::vector<SOCKET> recipients;

    {
        std::lock_guard<std::mutex> lock(room.mutex);
        auto& players = room.game.players;
        if (room.dissolved || players.empty()) {
            return;
        }

        std::vector<int> voteCount(players.size(), 0);
        for (const Player& player : players) {
            if (player.voted_target >= 0 &&
                player.voted_target < static_cast<int>(players.size())) {
                ++voteCount[player.voted_target];
            }
        }

        const int maxVotes = *std::max_element(voteCount.begin(), voteCount.end());
        int eliminatedIndex = -1;
        if (maxVotes > 0) {
            std::vector<int> tiedIndices;
            for (std::size_t i = 0; i < voteCount.size(); ++i) {
                if (voteCount[i] == maxVotes) {
                    tiedIndices.push_back(static_cast<int>(i));
                }
            }

            // 保留原有平票优先级：皮匠 > 好人 > 狼人。
            const auto priority = [&players](int index) {
                const std::string& role = players[index].current_role;
                if (role == "皮匠") return 3;
                if (is_good_team(role)) return 2;
                if (is_werewolf(role)) return 1;
                return 0;
            };
            std::sort(tiedIndices.begin(), tiedIndices.end(),
                      [&priority](int left, int right) {
                          return priority(left) > priority(right);
                      });
            eliminatedIndex = tiedIndices.front();
        }

        if (eliminatedIndex != -1) {
            const std::string& role = players[eliminatedIndex].current_role;
            if (role == "皮匠") {
                winner = "皮匠阵营胜利";
            } else if (is_werewolf(role)) {
                winner = "好人阵营胜利";
            } else {
                winner = "狼人阵营胜利";
            }
            consoleResult = "eliminated player " + std::to_string(eliminatedIndex + 1) +
                            " (" + role + "), " + winner;
        } else {
            winner = "狼人阵营胜利";
            consoleResult = "no player eliminated, " + winner;
        }

        for (std::size_t i = 0; i < players.size(); ++i) {
            if (!identities.empty()) identities += ';';
            identities += std::to_string(players[i].player_id) + ',' +
                          escapeProtocolField(players[i].name) + ',' +
                          escapeProtocolField(players[i].initial_role) + ',' +
                          escapeProtocolField(players[i].current_role) + ',' +
                          std::to_string(voteCount[i]);
            recipients.push_back(players[i].sock);
        }
        room.game.current_phase = GAME_OVER;
        room.game.voting_open = false;
    }

    const std::string resultMessage = "RESULT|" + escapeProtocolField(winner) + "|" + identities;
    for (SOCKET socket : recipients) {
        sendLine(room, socket, resultMessage);
    }
    std::cout << "[room " << room.code << "] " << consoleResult << '\n';

    std::this_thread::sleep_for(std::chrono::seconds(3));
    reset_game_state(room);
}
