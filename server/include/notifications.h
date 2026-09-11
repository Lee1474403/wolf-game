#pragma once

#include <string>

class Room;

// ======================== 游戏阶段的通知函数 ========================

// 在 PHASE_WEREWOLF 时主动告知所有狼人他们的队友信息
void notify_werewolves_of_teammates(Room& room);

// 通知爪牙狼人是哪几个玩家
void notify_minion_of_werewolves(Room& room);

// 在 PHASE_INSOMNIAC 时主动告知失眠者他们的最终身份
void notify_insomniacs_of_role(Room& room);
