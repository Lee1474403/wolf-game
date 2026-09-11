#include "game_types.h"
#include "notifications.h"
#include "game_rules.h"
#include "communication.h"
#include "room.h"
#include <string>

// 在 PHASE_WEREWOLF 时主动告知所有狼人他们的队友信息
void notify_werewolves_of_teammates(Room& room) {
    std::lock_guard<std::mutex> lock(room.mutex);
    auto& players = room.game.players;
    for (size_t i = 0; i < players.size(); ++i) {
        //std::string effective_role = players[i].doppel_copy_role.empty() ? players[i].initial_role : players[i].doppel_copy_role;
        
        if (is_werewolf(players[i].initial_role)) {
            // 统计其他狼人
            int werewolf_count = 0;
            std::string recognition_msg = "【狼人】狼人伙伴编号: ";
            for (size_t j = 0; j < players.size(); ++j) {
                if (j != i && is_werewolf(players[j].initial_role)) {
                    werewolf_count++;
                    recognition_msg += std::to_string(j + 1) + " ";
                }
            }
            
            // 如果没有其他狼人（单狼），告诉他可以看底牌
            if (werewolf_count == 0) {
                recognition_msg = "你是唯一的狼人，可以查看一张底牌";
            } else {
                recognition_msg = "你的狼人伙伴席位是: " + recognition_msg.substr(std::string("【狼人】狼人伙伴编号: ").size());
            }
            sendLine(room, players[i].sock,
                     "NOTICE|狼人互认|" + escapeProtocolField(recognition_msg));
            if (werewolf_count == 0) {
                sendAction(room, players[i].sock, "LONE_WOLF", "选择一张底牌查看");
            }
        }
    }
}

// 通知爪牙狼人是哪几个玩家
void notify_minion_of_werewolves(Room& room) {
    std::lock_guard<std::mutex> lock(room.mutex);
    auto& players = room.game.players;
    
    std::vector<std::string> wolf_ids;
    
    // 1. 严格检查初始身份 (initial_role)
    for (size_t i = 0; i < players.size(); ++i) {
        // 使用你的辅助函数 is_werewolf，但只传入初始身份
        if (is_werewolf(players[i].initial_role)) {
            wolf_ids.push_back(std::to_string(i + 1)); // 仅收集玩家编号
        }
    }

    // 2. 构造通知消息
    std::string msg;
    if (wolf_ids.empty()) {
        msg = "【系统】场上目前没有初始狼人（可能都在底牌中）。\n";
    } else {
        msg = "【系统】初始狼人玩家编号为: ";
        for (const auto& id : wolf_ids) {
            msg += id + " ";
        }
        msg += "\n";
    }

    // 3. 发送给所有爪牙或化身爪牙
    for (auto& p : players) {
        // 同样根据初始身份判断谁是爪牙
        if (p.initial_role == "爪牙" || p.doppel_copy_role == "爪牙") {
            sendLine(room, p.sock, "NOTICE|爪牙信息|" + escapeProtocolField(msg));
        }
    }
}

// 在 PHASE_INSOMNIAC 时主动告知失眠者他们的最终身份
void notify_insomniacs_of_role(Room& room) {
    std::lock_guard<std::mutex> lock(room.mutex);
    auto& players = room.game.players;
    for (size_t i = 0; i < players.size(); ++i) {
        std::string effective_role = players[i].doppel_copy_role.empty() ? players[i].initial_role : players[i].doppel_copy_role;
        
        if (is_insomniac(effective_role)) {
            // 失眠者最后确认自己的最终身份
            bool is_doppel = !players[i].doppel_copy_role.empty();
            std::string insomniac_msg = "【失眠者】你的最终身份是: " + players[i].current_role;
            
            if (is_doppel) {
                insomniac_msg += " (由幽灵化身的失眠者)\n";
            } else {
                insomniac_msg += "\n";
            }
            
            sendLine(room, players[i].sock,
                     "NOTICE|失眠者确认|" + escapeProtocolField(insomniac_msg));
        }
    }
}
