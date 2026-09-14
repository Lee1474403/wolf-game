#include "settlement_identity.h"

#include <algorithm>

std::string settlementEffectiveRole(const Player& player,
                                    const std::vector<Player>& players) {
    if (player.current_role != "幽灵") {
        return player.current_role;
    }

    const auto originalDoppel = std::find_if(
        players.begin(), players.end(), [](const Player& candidate) {
            return candidate.initial_role == "幽灵" &&
                   !candidate.doppel_copy_role.empty();
        });
    if (originalDoppel == players.end()) {
        return player.current_role;
    }

    return originalDoppel->doppel_copy_role;
}

std::string settlementRoleName(const Player& player,
                               const std::vector<Player>& players) {
    if (player.current_role != "幽灵") {
        return player.current_role;
    }

    const std::string effectiveRole = settlementEffectiveRole(player, players);
    return effectiveRole == "幽灵" ? "幽灵" : "幽灵-" + effectiveRole;
}
