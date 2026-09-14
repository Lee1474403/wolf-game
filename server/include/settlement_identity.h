#pragma once

#include "game_types.h"

#include <string>
#include <vector>

// 返回最终结算时参与阵营与胜负判定的有效身份。
// “幽灵”实体牌无论被谁持有，都视为本局复制到的角色。
std::string settlementEffectiveRole(const Player& player,
                                    const std::vector<Player>& players);

// 返回结算界面的身份名称；幽灵牌显示为“幽灵-有效身份”。
std::string settlementRoleName(const Player& player,
                               const std::vector<Player>& players);
