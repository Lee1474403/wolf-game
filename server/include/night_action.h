#pragma once

#include "room.h"
#include <string>

// ======================== 夜晚行动处理函数 ========================

// 处理夜晚阶段的各种行动
void process_night_action(Room& room, SOCKET clientSock, std::string cmd);
