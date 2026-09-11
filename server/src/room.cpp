#include "room.h"

#include <algorithm>
#include <utility>

Room::Room(std::string roomCode, int phaseSeconds, int discussionSeconds)
    : code(std::move(roomCode)),
      phase_seconds(phaseSeconds),
      discussion_seconds(discussionSeconds),
      last_activity(std::chrono::steady_clock::now()) {}

Player* Room::findPlayerUnlocked(SOCKET sock) {
    auto it = std::find_if(game.players.begin(), game.players.end(),
                           [sock](const Player& player) { return player.sock == sock; });
    return it == game.players.end() ? nullptr : &*it;
}

const Player* Room::findPlayerUnlocked(SOCKET sock) const {
    auto it = std::find_if(game.players.begin(), game.players.end(),
                           [sock](const Player& player) { return player.sock == sock; });
    return it == game.players.end() ? nullptr : &*it;
}

bool Room::isHostUnlocked(SOCKET sock) const {
    return host_socket == sock;
}

int Room::hostPlayerIdUnlocked() const {
    const Player* host = findPlayerUnlocked(host_socket);
    return host ? host->player_id : -1;
}

bool Room::allPlayersReadyUnlocked() const {
    return !game.players.empty() &&
           std::all_of(game.players.begin(), game.players.end(),
                       [](const Player& player) { return player.ready; });
}

void Room::touchUnlocked() {
    last_activity = std::chrono::steady_clock::now();
}

void Room::renumberPlayersUnlocked() {
    for (std::size_t i = 0; i < game.players.size(); ++i) {
        game.players[i].player_id = static_cast<int>(i) + 1;
    }
}

void Room::resetForNextRoundUnlocked() {
    ++generation;
    game.current_phase = JOINING;
    game.is_game_started = false;
    game.voting_open = false;
    game.current_deck.clear();
    game.table_cards.clear();
    game.table_cards_taken.clear();

    for (Player& player : game.players) {
        player.ready = false;
        player.initial_role.clear();
        player.current_role.clear();
        player.doppel_copy_role.clear();
        player.doppel_skill_used = false;
        player.is_revealed = false;
        player.drunk_took_card = false;
        player.has_acted = false;
        player.has_voted = false;
        player.voted_target = -1;
    }
    touchUnlocked();
}
