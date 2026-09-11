#pragma once

#include <string>
#include <vector>
#include <algorithm>

class Room;

// ======================== 游戏规则判断函数 ========================

// 判断是否是狼人
inline bool is_werewolf(const std::string& role) {
    return role == "狼人1" || role == "狼人2";
}

// 判断是否是好人阵营
inline bool is_good_team(const std::string& role) {
    static const std::vector<std::string> goods = {
        "捣蛋鬼", "强盗", "预言家", "酒鬼", "失眠者", "幽灵", "揭示者", "平民"
    };
    return std::find(goods.begin(), goods.end(), role) != goods.end();
}

// 判断是否是失眠者
inline bool is_insomniac(const std::string& role) {
    return role == "失眠者";
}

// 逻辑函数：根据玩家数量准备牌堆
void prepare_deck(Room& room, int player_count);
