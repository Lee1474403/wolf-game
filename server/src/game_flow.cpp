#include "game_flow.h"

#include "communication.h"
#include "notifications.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace {

bool waitForRoom(Room& room, std::uint64_t generation, int seconds) {
    if (seconds <= 0) {
        return true;
    }
    std::unique_lock<std::mutex> lock(room.mutex);
    const bool interrupted = room.state_changed.wait_for(
        lock, std::chrono::seconds(seconds),
        [&room, generation] { return room.dissolved || room.generation != generation; });
    return !interrupted;
}

bool beginPhase(Room& room, std::uint64_t generation, GamePhase phase) {
    std::lock_guard<std::mutex> lock(room.mutex);
    if (room.dissolved || room.generation != generation || !room.game.is_game_started) {
        return false;
    }
    room.game.current_phase = phase;
    for (Player& player : room.game.players) {
        player.has_acted = false;
    }
    return true;
}

bool runTimedPhase(Room& room, std::uint64_t generation, GamePhase phase,
                   const std::string& step, const std::string& publicPrompt) {
    if (!beginPhase(room, generation, phase)) {
        return false;
    }
    broadcastPhase(room, "NIGHT", step, publicPrompt);
    return true;
}

} // namespace

void print_final_identities(Room& room) {
    std::lock_guard<std::mutex> lock(room.mutex);
    std::cout << "\n[room " << room.code << "] final identities\n";
    for (std::size_t i = 0; i < room.game.players.size(); ++i) {
        const Player& player = room.game.players[i];
        std::cout << "  player " << (i + 1) << ": " << player.initial_role
                  << " -> " << player.current_role;
        if (player.initial_role == "幽灵" && !player.doppel_copy_role.empty()) {
            std::cout << " (copied " << player.doppel_copy_role << ')';
        }
        std::cout << '\n';
    }
    for (std::size_t i = 0; i < room.game.table_cards.size(); ++i) {
        std::cout << "  table " << (i + 1) << ": " << room.game.table_cards[i] << '\n';
    }
}

void game_flow_controller(std::shared_ptr<Room> room) {
    if (!room) {
        return;
    }

    std::uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(room->mutex);
        generation = room->generation;
    }

    if (!waitForRoom(*room, generation, std::min(5, room->phase_seconds))) return;

    if (!runTimedPhase(*room, generation, PHASE_DOPPEL, "DOPPEL", "幽灵请睁眼")) return;
    send_to_role(*room, "幽灵", "ACTION|COPY|请选择一名其他玩家复制初始身份");
    if (!waitForRoom(*room, generation, room->phase_seconds)) return;

    if (!runTimedPhase(*room, generation, PHASE_WEREWOLF, "WEREWOLF", "狼人请睁眼并确认同伴")) return;
    notify_werewolves_of_teammates(*room);
    if (!waitForRoom(*room, generation, room->phase_seconds)) return;

    if (!runTimedPhase(*room, generation, PHASE_MINION, "MINION", "爪牙请睁眼确认狼人")) return;
    notify_minion_of_werewolves(*room);
    if (!waitForRoom(*room, generation, room->phase_seconds)) return;

    if (!runTimedPhase(*room, generation, PHASE_SEER, "SEER", "预言家请选择查验目标")) return;
    send_to_role(*room, "预言家", "ACTION|SEER|查看一名玩家，或查看两张底牌");
    if (!waitForRoom(*room, generation, room->phase_seconds)) return;

    if (!runTimedPhase(*room, generation, PHASE_ROBBER, "ROBBER", "强盗请选择交换目标")) return;
    send_to_role(*room, "强盗", "ACTION|ROBBER|选择一名其他玩家交换身份，也可以选择跳过");
    if (!waitForRoom(*room, generation, room->phase_seconds)) return;

    if (!runTimedPhase(*room, generation, PHASE_TROUBLEMAKER, "TROUBLEMAKER", "捣蛋鬼请选择两名玩家")) return;
    send_to_role(*room, "捣蛋鬼", "ACTION|TROUBLEMAKER|选择两名其他玩家交换身份");
    if (!waitForRoom(*room, generation, room->phase_seconds)) return;

    if (!runTimedPhase(*room, generation, PHASE_DRUNK, "DRUNK", "酒鬼请选择一张底牌")) return;
    send_to_role(*room, "酒鬼", "ACTION|DRUNK|选择一张底牌与自己的牌交换");
    if (!waitForRoom(*room, generation, room->phase_seconds)) return;

    if (!runTimedPhase(*room, generation, PHASE_INSOMNIAC, "INSOMNIAC", "失眠者确认最终身份")) return;
    notify_insomniacs_of_role(*room);
    if (!waitForRoom(*room, generation, room->phase_seconds)) return;

    if (!runTimedPhase(*room, generation, PHASE_REVEALER, "REVEALER", "揭示者请选择查看目标")) return;
    send_to_role(*room, "揭示者", "ACTION|REVEALER|选择一名玩家进行揭示");
    if (!waitForRoom(*room, generation, room->phase_seconds)) return;

    std::string revealNotice;
    {
        std::lock_guard<std::mutex> lock(room->mutex);
        for (std::size_t i = 0; i < room->game.players.size(); ++i) {
            if (room->game.players[i].is_revealed) {
                if (!revealNotice.empty()) revealNotice += "；";
                revealNotice += "玩家 " + std::to_string(i + 1) + "：" +
                                room->game.players[i].current_role;
            }
        }
    }
    if (!revealNotice.empty()) {
        broadcast(*room, "NOTICE|揭示结果|" + escapeProtocolField(revealNotice));
    }

    print_final_identities(*room);
    {
        std::lock_guard<std::mutex> lock(room->mutex);
        if (room->dissolved || room->generation != generation) return;
        for (Player& player : room->game.players) {
            player.has_voted = false;
            player.voted_target = -1;
        }
        room->game.current_phase = DAY_DISCUSSION;
        room->game.voting_open = false;
    }
    broadcastPhase(*room, "DAY", "DISCUSSION", "天亮了，请讨论昨夜发生的事情");
    if (!waitForRoom(*room, generation, room->discussion_seconds)) return;

    {
        std::lock_guard<std::mutex> lock(room->mutex);
        if (room->dissolved || room->generation != generation) return;
        room->game.voting_open = true;
    }
    broadcastPhase(*room, "DAY", "VOTE", "讨论结束，请投出你认为的狼人");
    broadcast(*room, "ACTION|VOTE|选择一名玩家并提交选票");
}

void reset_game_state(Room& room) {
    {
        std::lock_guard<std::mutex> lock(room.mutex);
        if (room.dissolved) {
            return;
        }
        room.resetForNextRoundUnlocked();
    }
    room.state_changed.notify_all();
    broadcast(room, "GAME_RESET|本局已结束，房间已开放下一局准备");
    broadcastRoomStatus(room);
    std::cout << "[room " << room.code << "] reset for next round\n";
}
