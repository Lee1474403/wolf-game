#include "room_manager.h"
#include "settlement_identity.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main() {
    std::cout << "test: create manager" << std::endl;
    RoomManager manager(0, 1, 0, 0);

    std::cout << "test: first room" << std::endl;
    JoinResult first = manager.join("1234", "房主", static_cast<SOCKET>(101));
    assert(first.status == JoinStatus::Joined);
    assert(first.created);
    assert(first.is_host);
    assert(first.player_id == 1);

    std::cout << "test: same room" << std::endl;
    JoinResult sameRoom = manager.join("1234", "玩家二", static_cast<SOCKET>(102));
    assert(sameRoom.status == JoinStatus::Joined);
    assert(!sameRoom.created);
    assert(sameRoom.room == first.room);
    assert(sameRoom.player_id == 2);

    std::cout << "test: other room" << std::endl;
    JoinResult otherRoom = manager.join("5678", "另一房主", static_cast<SOCKET>(201));
    assert(otherRoom.status == JoinStatus::Joined);
    assert(otherRoom.room != first.room);
    assert(manager.roomCount() == 2);

    std::cout << "test: fill room" << std::endl;
    for (int i = 3; i <= 9; ++i) {
        JoinResult joined = manager.join("1234", "玩家" + std::to_string(i),
                                         static_cast<SOCKET>(100 + i));
        assert(joined.status == JoinStatus::Joined);
    }
    JoinResult full = manager.join("1234", "第十人", static_cast<SOCKET>(110));
    assert(full.status == JoinStatus::RoomFull);

    std::cout << "test: room state" << std::endl;
    {
        std::lock_guard<std::mutex> lock(first.room->mutex);
        assert(first.room->game.players.size() == 9);
        assert(first.room->hostPlayerIdUnlocked() == 1);
        assert(!first.room->allPlayersReadyUnlocked());
        for (Player& player : first.room->game.players) player.ready = true;
        assert(first.room->allPlayersReadyUnlocked());
        first.room->game.is_game_started = true;
    }
    std::cout << "test: reject running" << std::endl;
    JoinResult running = manager.join("1234", "迟到玩家", static_cast<SOCKET>(111));
    assert(running.status == JoinStatus::GameInProgress);

    std::cout << "test: validation" << std::endl;
    assert(manager.join("12A4", "错误房间", static_cast<SOCKET>(301)).status ==
           JoinStatus::InvalidCode);
    assert(manager.join("0000", "", static_cast<SOCKET>(302)).status ==
           JoinStatus::InvalidName);

    std::cout << "test: doppel settlement identity" << std::endl;
    std::vector<Player> settlementPlayers(2);
    settlementPlayers[0].initial_role = "幽灵";
    settlementPlayers[0].current_role = "幽灵";
    settlementPlayers[0].doppel_copy_role = "预言家";
    settlementPlayers[1].initial_role = "平民";
    settlementPlayers[1].current_role = "平民";
    assert(settlementRoleName(settlementPlayers[0], settlementPlayers) ==
           "幽灵-预言家");
    assert(settlementEffectiveRole(settlementPlayers[0], settlementPlayers) ==
           "预言家");

    settlementPlayers[0].doppel_copy_role = "强盗";
    settlementPlayers[0].current_role = "狼人1";
    settlementPlayers[1].current_role = "幽灵";
    assert(settlementRoleName(settlementPlayers[0], settlementPlayers) == "狼人1");
    assert(settlementRoleName(settlementPlayers[1], settlementPlayers) ==
           "幽灵-强盗");
    assert(settlementEffectiveRole(settlementPlayers[1], settlementPlayers) ==
           "强盗");

    settlementPlayers[0].doppel_copy_role = "狼人1";
    assert(settlementEffectiveRole(settlementPlayers[1], settlementPlayers) ==
           "狼人1");
    assert(settlementRoleName(settlementPlayers[1], settlementPlayers) ==
           "幽灵-狼人1");

    settlementPlayers[0].doppel_copy_role = "皮匠";
    assert(settlementEffectiveRole(settlementPlayers[1], settlementPlayers) ==
           "皮匠");
    assert(settlementRoleName(settlementPlayers[1], settlementPlayers) ==
           "幽灵-皮匠");

    settlementPlayers[0].doppel_copy_role.clear();
    assert(settlementRoleName(settlementPlayers[1], settlementPlayers) == "幽灵");
    assert(settlementEffectiveRole(settlementPlayers[1], settlementPlayers) ==
           "幽灵");

    std::cout << "room manager tests passed\n";
    return 0;
}
