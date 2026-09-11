#include "game_types.h"
#include "game_rules.h"
#include "room.h"

// 逻辑函数：根据玩家数量准备牌堆
void prepare_deck(Room& room, int player_count) {
    auto& current_deck = room.game.current_deck;
    const auto& base_cards = room.game.base_cards;
    current_deck = base_cards;
    
    // 基础逻辑：总牌数 = 玩家数 + 3
    // 7人：需要10张牌（base_cards 7张 + 3张备选）
    // 8人：需要11张牌（增加 揭示者）
    // 9人：需要12张牌（增加 揭示者 + 平民）
    
    if (player_count == 7) {
        current_deck.push_back("失眠者");
        current_deck.push_back("幽灵");
        current_deck.push_back("皮匠");
    } else if (player_count == 8) {
        current_deck.push_back("失眠者");
        current_deck.push_back("幽灵");
        current_deck.push_back("皮匠");
        current_deck.push_back("揭示者");
    } else if (player_count == 9) {
        current_deck.push_back("失眠者");
        current_deck.push_back("幽灵");
        current_deck.push_back("皮匠");
        current_deck.push_back("揭示者");
        current_deck.push_back("平民");
    }
}
