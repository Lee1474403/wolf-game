#include "room_manager.h"

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

    std::cout << "room manager tests passed\n";
    return 0;
}
