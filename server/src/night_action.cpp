#include "game_types.h"
#include "night_action.h"
#include "game_rules.h"
#include "communication.h"
#include <string>
#include <iostream>    // 用于 std::cout, std::cerr
#include <ostream>     // 用于 std::endl
#include <sstream>     // 用于 std::stringstream (解析多参数指令必备)
#include <algorithm>   // 如果你用到了 std::all_of 或 std::swap 也需要它

// 处理夜晚阶段的各种行动
void process_night_action(Room& room, SOCKET clientSock, std::string cmd) {
    auto& mtx = room.mutex;
    auto& players = room.game.players;
    auto& current_phase = room.game.current_phase;
    auto& table_cards = room.game.table_cards;

    // 旧逻辑中的 send 调用统一通过房间发送锁，避免阶段通知和技能反馈交叉写包。
#define send(socket, data, length, flags) sendLine(room, (socket), std::string(data))

    std::lock_guard<std::mutex> lock(mtx);
    
    // 清理末尾的空格和换行（安全检查避免 npos + 1 溢出）
    std::cout << "[DEBUG] 原始指令: [" << cmd << "]" << std::endl;
    {
        size_t end_pos = cmd.find_last_not_of(" \n\r\t");
        if (end_pos != std::string::npos) {
            cmd.erase(end_pos + 1);
        } else {
            // 如果整个字符串都是空格/换行，直接返回
            std::cout << "[DEBUG] 指令为空或全是空格，返回" << std::endl;
            return;
        }
    }
    std::cout << "[DEBUG] 清理后指令: [" << cmd << "]" << std::endl;
    
    // 找到该玩家
    Player* pPlayer = nullptr;
    int playerIndex = -1;
    for (size_t i = 0; i < players.size(); ++i) {
        if (players[i].sock == clientSock) {
            pPlayer = &players[i];
            playerIndex = (int)i;
            break;
        }
    }
    
    std::cout << "[DEBUG] 玩家查找: playerIndex=" << playerIndex << ", sock匹配=" << (pPlayer != nullptr ? "是" : "否") << std::endl;
    
    if (!pPlayer) {
        std::cout << "[DEBUG] 未找到对应玩家，返回" << std::endl;
        return;
    }
    std::cout << "[DEBUG] 玩家身份: " << pPlayer->initial_role << ", 当前阶段: " << current_phase << std::endl;
    
    // ============= 阶段 1: 幽灵 (化身幽灵) =============
    if (current_phase == PHASE_DOPPEL && pPlayer->initial_role == "幽灵") {
        
        // --- 逻辑 A: 执行复制动作 ---
        if (cmd.find("COPY") == 0) {
            if (pPlayer->has_acted) {
                send(clientSock, "【错误】你已经复制过身份了。\n", 33, 0);
                return;
            }

            // 解析目标编号
            int target_id = -1;
            try {
                size_t space_pos = cmd.find(' ');
                if (space_pos != std::string::npos) {
                    target_id = std::stoi(cmd.substr(space_pos + 1)) - 1;
                }
            } catch (...) {
                send(clientSock, "【错误】指令格式：COPY [编号]\n", 34, 0);
                return;
            }

            // 验证目标
            if (target_id < 0 || target_id >= (int)players.size() || target_id == playerIndex) {
                send(clientSock, "【错误】无效的目标编号。\n", 27, 0);
                return;
            }

            // 核心：复制初始身份
            std::string target_role = players[target_id].initial_role;
            pPlayer->doppel_copy_role = target_role;
            pPlayer->has_acted = true; // 标记复制动作完成

            std::string msg = "【幽灵】你已复制 " + std::to_string(target_id + 1) + " 号的身份: " + target_role + "\n";
            send(clientSock, msg.c_str(), (int)msg.length(), 0);
            sendLine(room, clientSock, "COPIED_ROLE|" + escapeProtocolField(target_role));

            // 判定后续引导
            if (target_role == "预言家") {
                sendAction(room, clientSock, "SEER", "立即使用复制到的预言家能力");
                send(clientSock, "【提示】请立即输入：VIEW_PLAYER [编号] 或 VIEW_TABLE [n1] [n2]\n", 67, 0);
            } else if (target_role == "强盗") {
                sendAction(room, clientSock, "ROBBER", "立即使用复制到的强盗能力");
                send(clientSock, "【提示】请立即输入：ROB [编号]\n", 40, 0);
            } else if (target_role == "捣蛋鬼") {
                sendAction(room, clientSock, "TROUBLEMAKER", "立即使用复制到的捣蛋鬼能力");
                send(clientSock, "【提示】请立即输入：SWAP [编号1] [编号2]\n", 49, 0);
            } else if (target_role == "酒鬼") {
                sendAction(room, clientSock, "DRUNK", "立即使用复制到的酒鬼能力");
                send(clientSock, "【提示】请立即输入：DRINK [底牌编号1-3]\n", 44, 0);
            } else if (target_role == "揭示者") {
                sendAction(room, clientSock, "REVEALER", "立即使用复制到的揭示者能力");
                send(clientSock, "【提示】请立即输入：REVEAL [编号]\n", 41, 0);
            } else {
                sendLine(room, clientSock, "WAITING|该身份没有即时行动，请等待其他玩家");
                send(clientSock, "【提示】该身份无即时行动，请闭眼等待天亮。\n", 46, 0);
            }
            return;
        }

        // --- 逻辑 B: 执行即时技能 (仅限 COPY 之后，且 skill 未使用时) ---
        if (pPlayer->has_acted && !pPlayer->doppel_skill_used) {
            std::string role = pPlayer->doppel_copy_role;

            // 幽灵-预言家
            if (role == "预言家") {
                // 选项 A: 查看一名玩家
                if (cmd.find("VIEW_PLAYER") == 0) {
                    int tid = -1;
                    try { tid = std::stoi(cmd.substr(12)) - 1; } catch(...) { 
                        send(clientSock, "【提示】格式错误，请使用: VIEW_PLAYER [编号]\n", 50, 0);
                        return; 
                    }

                    if (tid >= 0 && tid < (int)players.size() && tid != playerIndex) {
                        std::string r = players[tid].initial_role; 
                        std::string m = "【幽灵-预言家】玩家 " + std::to_string(tid+1) + " 的初始身份是: " + r + "\n";
                        send(clientSock, m.c_str(), (int)m.length(), 0);
                        pPlayer->doppel_skill_used = true;
                    } else {
                        send(clientSock, "【错误】无效的目标玩家编号。\n", 40, 0);
                    }
                } 
                // 选项 B: 查看底牌 (规则通常为查看 3 张底牌中的 2 张)
                else if (cmd.find("VIEW_TABLE") == 0) {
                    // 允许玩家输入 VIEW_TABLE 1 2 或默认为 1 2
                    int idx1 = 0, idx2 = 1; 
                    // 增加解析逻辑... (此处简化处理)
                    std::string m = "【幽灵-预言家】你查看了底牌 " + std::to_string(idx1+1) + " 和 " + std::to_string(idx2+1) + 
                                    "，分别是: " + table_cards[idx1] + " 和 " + table_cards[idx2] + "\n";
                    send(clientSock, m.c_str(), (int)m.length(), 0);
                    pPlayer->doppel_skill_used = true;
                }
            }
            // 幽灵-强盗
            else if (role == "强盗" && cmd.find("ROB") == 0) {
                int tid = -1;
                try { 
                    // 自动清洗空格并解析
                    std::string target_str = cmd.substr(4);
                    target_str.erase(0, target_str.find_first_not_of(" "));
                    tid = std::stoi(target_str) - 1; 
                } catch(...) { 
                    send(clientSock, "【错误】指令格式为: ROB [编号]\n", 40, 0);
                    return; 
                }

                // 严谨校验：编号在范围内 且 不是自己
                if (tid >= 0 && tid < (int)players.size() && tid != playerIndex) {
                    // --- 开始交换逻辑 ---
                    
                    // A. 记录目标当前的身份（这才是幽灵最终要变成的身份）
                    std::string target_role = players[tid].current_role;
                    
                    // B. 目标身份变为“幽灵”（强盗把自己的“身份”留给对方）
                    // 注意：这里给对方的是 pPlayer->current_role，此时它还是“幽灵”
                    players[tid].current_role = pPlayer->current_role; 
                    
                    // C. 幽灵玩家身份变为目标的新身份
                    pPlayer->current_role = target_role;
                    
                    // --- 反馈结果 ---
                    std::string m = "【幽灵-强盗】动作完成！\n";
                    m += "你偷取了 " + std::to_string(tid + 1) + " 号玩家的牌。\n";
                    m += "🔑 你现在的新身份是: 【" + pPlayer->current_role + "】\n";
                    
                    send(clientSock, m.c_str(), (int)m.length(), 0);
                    
                    // 标记技能已使用，防止重复执行
                    pPlayer->doppel_skill_used = true;
                    
                    // 服务器后台记录，方便调试
                    std::cout << "[LOG] 幽灵玩家(" << playerIndex + 1 << ") 复制强盗并偷取了 " 
                            << tid + 1 << " 号, 最终变为 " << target_role << std::endl;
                } else {
                    std::string err = (tid == playerIndex) ? "【错误】你不能偷自己的牌！\n" : "【错误】无效的玩家编号。\n";
                    send(clientSock, err.c_str(), (int)err.length(), 0);
                }
            }
            // 幽灵-酒鬼
            else if (role == "酒鬼" && cmd.find("DRINK") == 0) {
                int table_idx = -1;
                try {
                    // 提取指令后的内容，例如 "DRINK 2"
                    std::string params = cmd.substr(5); 
                    params.erase(0, params.find_first_not_of(" ")); // 去除前导空格
                    if (params.empty()) throw std::invalid_argument("missing param");
                    table_idx = std::stoi(params) - 1; // 转换为 0 索引
                } catch (...) {
                    send(clientSock, "【错误】格式错误！用法: DRINK [底牌编号1-3]\n", 50, 0);
                    return;
                }

                // --- 健壮性校验 ---
                // 1. 校验底牌编号是否在有效范围内 (通常底牌有 3 张)
                if (table_idx < 0 || table_idx >= (int)table_cards.size()) {
                    std::string err = "【错误】无效的底牌编号。当前可选: 1-" + std::to_string(table_cards.size()) + "\n";
                    send(clientSock, err.c_str(), (int)err.length(), 0);
                    return;
                }

                // --- 执行核心交换逻辑 ---
                // 注意：酒鬼的规则是“交换但不准看”，所以我们只进行数据交换，不回传身份字符串
                
                // 获取当前的 current_role (此时应为 "幽灵")
                std::string old_role = pPlayer->current_role;
                
                // 与底牌进行物理交换
                pPlayer->current_role = table_cards[table_idx];
                table_cards[table_idx] = old_role;

                // --- 状态维护 ---
                pPlayer->doppel_skill_used = true;
                
                // 给玩家的反馈（严禁告诉他换到了什么！）
                std::string success_msg = "【幽灵-酒鬼】你已醉醺醺地与第 " + std::to_string(table_idx + 1) + " 张底牌进行了交换。\n";
                success_msg += "你现在不知道自己是谁，请等待讨论阶段开始。\n";
                send(clientSock, success_msg.c_str(), (int)success_msg.length(), 0);

                // 服务器后台记录
                std::cout << "[LOG] 幽灵玩家(" << playerIndex + 1 << ") 复制酒鬼，与底牌 " 
                        << table_idx + 1 << " 交换。新身份已隐藏。" << std::endl;
            }
            // 3. 幽灵-捣蛋鬼
            else if (role == "捣蛋鬼" && cmd.find("SWAP") == 0) {
                std::vector<int> targets;
                try {
                    // 1. 提取指令后的内容并使用 stringstream 分词，自动过滤任意多余空格
                    std::string params = cmd.substr(4); 
                    std::stringstream ss(params);
                    std::string temp;
                    while (ss >> temp) {
                        // 确保提取的是纯数字
                        if (!temp.empty() && std::all_of(temp.begin(), temp.end(), ::isdigit)) {
                            targets.push_back(std::stoi(temp) - 1);
                        } else {
                            throw std::invalid_argument("invalid char");
                        }
                    }
                } catch (...) {
                    send(clientSock, "【错误】格式错误！用法: SWAP [编号1] [编号2] (例如: SWAP 2 5)\n", 65, 0);
                    return;
                }

                // 2. 数量校验
                if (targets.size() != 2) {
                    send(clientSock, "【错误】必须选择且仅能选择 2 名玩家进行交换。\n", 50, 0);
                    return;
                }

                int t1 = targets[0];
                int t2 = targets[1];

                // 3. 业务规则校验
                // - 编号必须在有效范围内
                // - 不能交换自己 (捣蛋鬼不能动自己的牌)
                // - 两个目标不能是同一个人
                if (t1 < 0 || t1 >= (int)players.size() || t2 < 0 || t2 >= (int)players.size()) {
                    send(clientSock, "【错误】玩家编号超出范围。\n", 30, 0);
                } else if (t1 == playerIndex || t2 == playerIndex) {
                    send(clientSock, "【错误】作为捣蛋鬼，你不能交换自己的牌。\n", 50, 0);
                } else if (t1 == t2) {
                    send(clientSock, "【错误】不能选择两个相同的编号。\n", 40, 0);
                } else {
                    // --- 执行核心交换逻辑 ---
                    std::swap(players[t1].current_role, players[t2].current_role);

                    // 反馈给玩家
                    std::string m = "【幽灵-捣蛋鬼】交换成功！玩家 " + std::to_string(t1+1) + 
                                    " 与玩家 " + std::to_string(t2+1) + " 的身份已互换。\n";
                    send(clientSock, m.c_str(), (int)m.length(), 0);

                    // 标记技能已使用
                    pPlayer->doppel_skill_used = true;
                    
                    std::cout << "[LOG] 幽灵-捣蛋鬼交换了 " << t1+1 << " 和 " << t2+1 << " 的位置。" << std::endl;
                }
            }
            // 4. 幽灵-揭示者
            else if (role == "揭示者" && cmd.find("REVEAL") == 0) {
                int tid = -1;
                try { 
                    std::string sub = cmd.substr(6); // 截取 "REVEAL" 之后的部分
                    // 清洗空格
                    sub.erase(0, sub.find_first_not_of(" "));
                    tid = std::stoi(sub) - 1; 
                } catch(...) { 
                    send(clientSock, "【错误】格式不正确。用法: REVEAL [编号]\n", 45, 0);
                    return; 
                }

                // 校验：不能看自己，且编号有效
                if (tid >= 0 && tid < (int)players.size() && tid != playerIndex) {
                    std::string target_current_role = players[tid].current_role;
                    
                    // --- 核心判定逻辑 ---
                    // 规则：如果是好人阵营（排除狼人和皮匠），天亮时翻牌
                    if (is_good_team(target_current_role) && target_current_role != "皮匠") {
                        // 标记该玩家，天亮时上帝线程会统一广播
                        players[tid].is_revealed = true; 
                        
                        std::string m = "【幽灵-揭示者】你查看了 " + std::to_string(tid+1) + " 号，身份为: " + 
                                        target_current_role + "。该身份将在天亮时向全场公示。\n";
                        send(clientSock, m.c_str(), (int)m.length(), 0);
                    } 
                    else {
                        // 如果是坏人或中立，揭示者自己知道身份，但不会翻牌
                        std::string m = "【幽灵-揭示者】你查看了 " + std::to_string(tid+1) + " 号，身份为: " + 
                                        target_current_role + "。由于其身份特殊，天亮后不会公示。\n";
                        send(clientSock, m.c_str(), (int)m.length(), 0);
                    }

                    // 技能使用完毕
                    pPlayer->doppel_skill_used = true;
                    
                    std::cout << "[LOG] 幽灵-揭示者(" << playerIndex+1 << ") 查看了 " << tid+1 
                            << " 号 (" << target_current_role << ")" << std::endl;
                } 
                else {
                    std::string err = (tid == playerIndex) ? "【错误】你不能揭示自己的牌。\n" : "【错误】无效的玩家编号。\n";
                    send(clientSock, err.c_str(), (int)err.length(), 0);
                }
            }
        }
    }
    
    // ============= 阶段 2: 狼人 (Werewolf) =============
    else if (current_phase == PHASE_WEREWOLF && is_werewolf(pPlayer->initial_role)) {
        std::cout << "[DEBUG] === 进入狼人(WEREWOLF)处理逻辑 ===" << std::endl;
        
        // 1. 统计场上狼人的总数 (注：已于函数入口获取 mtx，无需重复加锁)
        int total_wolf_count = 0;
        for (const auto& p : players) {
            if (is_werewolf(p.initial_role)) {
                total_wolf_count++;
            }
        }
        
        std::cout << "[DEBUG] 狼人数量: " << total_wolf_count << ", 指令: [" << cmd << "]" << std::endl;

        // 2. 只有"单狼"逻辑
        if (total_wolf_count == 1) {
            std::cout << "[DEBUG] 这是单狼局面" << std::endl;
            
            // 解析指令：VIEW_TABLE [index]
            if (cmd.find("VIEW_TABLE") == 0) {
                std::cout << "[DEBUG] 匹配 VIEW_TABLE 指令" << std::endl;
                if (pPlayer->has_acted) {
                    std::cout << "[DEBUG] 已经行动过，拒绝" << std::endl;
                    std::string msg = "【提示】你已经执行过行动了。\n";
                    int ret = send(clientSock, msg.c_str(), (int)msg.length(), 0);
                    std::cout << "[DEBUG] send() ret=" << ret << std::endl;
                    return;
                }

                // 提取索引 (例如 "VIEW_TABLE 2" 提取出 2)
                std::cout << "[DEBUG] 开始解析索引..." << std::endl;
                try {
                    size_t space_pos = cmd.find_last_of(' ');
                    std::cout << "[DEBUG] space_pos=" << (space_pos == std::string::npos ? -1 : (int)space_pos) << std::endl;
                    
                    if (space_pos != std::string::npos) {
                        std::string idx_str = cmd.substr(space_pos + 1);
                        std::cout << "[DEBUG] idx_str=[" << idx_str << "]" << std::endl;
                        
                        int idx = std::stoi(idx_str);
                        std::cout << "[DEBUG] idx=" << idx << std::endl;
                        
                        if (idx >= 1 && idx <= 3) {
                            std::string role = table_cards[idx - 1];
                            std::string msg = "【独狼】你查看了第 " + std::to_string(idx) + " 张底牌，身份是: " + role + "\n";
                            std::cout << "[DEBUG] 发送: " << msg << std::endl;
                            
                            int ret = send(clientSock, msg.c_str(), (int)msg.length(), 0);
                            std::cout << "[DEBUG] send()返回=" << ret << ", errno=" << errno << std::endl;
                            
                            pPlayer->has_acted = true; // 行动完成
                        } else {
                            std::cout << "[DEBUG] idx超出范围: " << idx << std::endl;
                            std::string msg = "【系统】底牌索引错误，请输入 1, 2 或 3。\n";
                            int ret = send(clientSock, msg.c_str(), (int)msg.length(), 0);
                            std::cout << "[DEBUG] send()返回=" << ret << std::endl;
                        }
                    } else {
                        send(clientSock, "【提示】请输入具体查看哪张，例如: VIEW_TABLE 1\n", 60, 0);
                    }
                } catch (...) {
                    std::cout << "[ERROR] 解析 VIEW_TABLE 指令异常!" << std::endl;
                    std::string msg = "【系统】指令格式错误。示例: VIEW_TABLE 1\n";
                    int ret = send(clientSock, msg.c_str(), (int)msg.length(), 0);
                    std::cout << "[DEBUG] send() 异常处理返回值: " << ret << std::endl;
                }
            } else {
                std::cout << "[DEBUG] 独狼收到非 VIEW_TABLE 指令: [" << cmd << "]" << std::endl;
            }
        } else {
            std::cout << "[DEBUG] 不是单狼局面，狼人总数: " << total_wolf_count << std::endl;
        }
    }
    // ============= 阶段 4: 预言家 (Seer) =============
    else if (current_phase == PHASE_SEER && pPlayer->initial_role == "预言家") {
        
        // 1. 检查是否已经行动过
        if (pPlayer->has_acted) {
            // 如果玩家已经发送过查看指令，不再处理
            return; 
        }

        // --- 逻辑 A: 查看一名玩家的身份 (VIEW_PLAYER [编号]) ---
        if (cmd.find("VIEW_PLAYER") == 0) {
            int target_id = -1;
            try {
                size_t space_pos = cmd.find(' ');
                if (space_pos != std::string::npos) {
                    target_id = std::stoi(cmd.substr(space_pos + 1)) - 1;
                }
            } catch (...) {
                send(clientSock, "【错误】指令格式：VIEW_PLAYER [编号]\n", 39, 0);
                return;
            }

            // 验证目标合法性（不能看自己）
            if (target_id < 0 || target_id >= (int)players.size() || target_id == playerIndex) {
                send(clientSock, "【错误】无效的目标编号。\n", 27, 0);
                return;
            }

            // 预言家看牌逻辑：通常看的是 initial_role（确保看到的是最初分配的身份）
            // 也可以根据你的村规改为 current_role（但强盗/捣蛋鬼还没行动，此时两者通常一致）
            std::string target_role = players[target_id].initial_role;
            std::string msg = "【预言家】玩家 " + std::to_string(target_id + 1) + " 的身份是: " + target_role + "\n";
            send(clientSock, msg.c_str(), (int)msg.length(), 0);

            pPlayer->has_acted = true; // 标记行动结束
        }

        // --- 逻辑 B: 查看两张底牌 (VIEW_TABLE [编号1] [编号2]) ---
        else if (cmd.find("VIEW_TABLE") == 0) {
            int c1 = -1, c2 = -1;
            try {
                // 解析：VIEW_TABLE 1 2
                size_t s1 = cmd.find(' ');
                size_t s2 = cmd.find(' ', s1 + 1);
                if (s1 != std::string::npos && s2 != std::string::npos) {
                    c1 = std::stoi(cmd.substr(s1 + 1, s2 - s1 - 1)) - 1;
                    c2 = std::stoi(cmd.substr(s2 + 1)) - 1;
                }
            } catch (...) {
                // 解析失败则默认看前两张
                c1 = 0; c2 = 1;
            }

            // 验证底牌索引（底牌通常为 0, 1, 2）
            if (c1 >= 0 && c1 < 3 && c2 >= 0 && c2 < 3 && c1 != c2) {
                std::string msg = "【预言家】你查看的底牌是：[" + std::to_string(c1 + 1) + "]" + table_cards[c1] + 
                                " 和 [" + std::to_string(c2 + 1) + "]" + table_cards[c2] + "\n";
                send(clientSock, msg.c_str(), (int)msg.length(), 0);
                pPlayer->has_acted = true;
            } else {
                send(clientSock, "【错误】请选择两张不同的底牌编号 (1-3)。\n", 44, 0);
            }
        }
    }
    
    // ============= 阶段 5: 强盗 (Robber) =============
    else if (current_phase == PHASE_ROBBER && pPlayer->initial_role == "强盗") {
        
        if (cmd.find("ROB") == 0) {
            // 1. 检查是否已经行动过
            if (pPlayer->has_acted) {
                send(clientSock, "【错误】你在这个阶段已经行动过了。\n", 35, 0);
                return;
            }

            // 2. 解析目标编号
            int target_id = -1;
            try {
                size_t space_pos = cmd.find(' ');
                if (space_pos != std::string::npos) {
                    target_id = std::stoi(cmd.substr(space_pos + 1)) - 1;
                }
            } catch (...) {
                send(clientSock, "【错误】指令格式：ROB [编号]\n", 29, 0);
                return;
            }

            // 3. 验证目标有效性 (不能抢自己)
            if (target_id < 0 || target_id >= (int)players.size() || target_id == playerIndex) {
                send(clientSock, "【错误】无效的目标编号（不能选择自己）。\n", 41, 0);
                return;
            }

            // 4. 执行交换逻辑
            // 强盗拿走目标的当前身份，目标变成强盗（通常目标不知道自己变了）
            std::string target_current = players[target_id].current_role;
            std::string my_current = pPlayer->current_role;

            pPlayer->current_role = target_current;
            players[target_id].current_role = my_current;

            // 5. 标记行动结束并通知结果
            pPlayer->has_acted = true;
            
            std::string rob_msg = "【强盗】你已偷取 " + std::to_string(target_id + 1) + 
                                " 号玩家的牌。你现在的身份是: " + pPlayer->current_role + "\n";
            send(clientSock, rob_msg.c_str(), (int)rob_msg.length(), 0);
            
            send(clientSock, "【提示】行动完毕，请闭眼。\n", 28, 0);
        }
        // 如果强盗选择不行动（有些规则允许，但通常强盗是强制/建议行动的）
        else if (cmd == "PASS") {
            pPlayer->has_acted = true;
            send(clientSock, "【强盗】你选择了不进行偷取，身份保持不变。\n", 42, 0);
        }
    }
    
    // ============= 阶段 6: 捣蛋鬼 (Troublemaker) =============
    else if (current_phase == PHASE_TROUBLEMAKER && pPlayer->initial_role == "捣蛋鬼") {
        
        if (cmd.find("SWAP") == 0) {
            // 1. 检查是否已经行动过
            if (pPlayer->has_acted) {
                send(clientSock, "【错误】你在这个阶段已经行动过了。\n", 35, 0);
                return;
            }

            int target1 = -1, target2 = -1;
            try {
                // 解析指令格式：SWAP [编号1] [编号2]
                size_t pos1 = cmd.find(' ');
                size_t pos2 = cmd.find(' ', pos1 + 1);
                if (pos1 != std::string::npos && pos2 != std::string::npos) {
                    target1 = std::stoi(cmd.substr(pos1 + 1, pos2 - pos1 - 1)) - 1;
                    target2 = std::stoi(cmd.substr(pos2 + 1)) - 1;
                } else {
                    send(clientSock, "【错误】格式错误。请输入：SWAP [编号1] [编号2]\n", 49, 0);
                    return;
                }
            } catch (...) {
                send(clientSock, "【错误】请输入有效的玩家编号数字。\n", 35, 0);
                return;
            }

            // 2. 验证目标有效性
            // 规则：不能交换自己，两个目标不能相同，编号必须在范围内
            if (target1 < 0 || target1 >= (int)players.size() || 
                target2 < 0 || target2 >= (int)players.size() || 
                target1 == playerIndex || target2 == playerIndex || 
                target1 == target2) {
                send(clientSock, "【错误】无效的目标编号。你不能交换自己，且必须选择两个不同的他人。\n", 71, 0);
                return;
            }

            // 3. 执行交换逻辑 (交换 current_role)
            std::string temp_role = players[target1].current_role;
            players[target1].current_role = players[target2].current_role;
            players[target2].current_role = temp_role;

            // 4. 标记行动结束
            pPlayer->has_acted = true;

            // 5. 通知结果 (捣蛋鬼不知道具体的身份，只确认交换成功)
            std::string msg = "【捣蛋鬼】交换成功！你已交换了 " + std::to_string(target1 + 1) + 
                            " 号和 " + std::to_string(target2 + 1) + " 号玩家的牌。\n";
            send(clientSock, msg.c_str(), (int)msg.length(), 0);
            
            send(clientSock, "【提示】行动完毕，请闭眼。\n", 28, 0);
        }
    }
    
    // ============= 阶段 7: 酒鬼 (Drunk) =============
    else if (current_phase == PHASE_DRUNK && pPlayer->initial_role == "酒鬼") {
        
        if (cmd.find("DRINK") == 0) {
            // 1. 检查是否已经行动过
            if (pPlayer->has_acted) {
                send(clientSock, "【错误】你已经喝醉并换过牌了。\n", 33, 0);
                return;
            }

            // 2. 解析底牌编号 (1-3)
            int table_card_idx = -1;
            try {
                size_t space_pos = cmd.find(' ');
                if (space_pos != std::string::npos) {
                    table_card_idx = std::stoi(cmd.substr(space_pos + 1)) - 1;
                }
            } catch (...) {
                send(clientSock, "【错误】指令格式：DRINK [底牌编号1-3]\n", 40, 0);
                return;
            }

            // 3. 验证底牌索引有效性
            if (table_card_idx < 0 || table_card_idx >= 3) {
                send(clientSock, "【错误】无效的底牌编号。请选择 1, 2 或 3。\n", 44, 0);
                return;
            }

            // 4. 执行交换逻辑 (与底牌交换 current_role)
            // 注意：酒鬼不知道自己换到了什么，所以不查询具体身份字符串给玩家
            std::string my_old_role = pPlayer->current_role;
            pPlayer->current_role = table_cards[table_card_idx];
            table_cards[table_card_idx] = my_old_role;

            // 5. 标记行动结束
            pPlayer->has_acted = true;

            // 6. 通知结果
            std::string msg = "【酒鬼】你已将自己的牌与第 " + std::to_string(table_card_idx + 1) + 
                            " 张底牌交换。你现在已经不知道自己是谁了...\n";
            send(clientSock, msg.c_str(), (int)msg.length(), 0);
            
            send(clientSock, "【提示】行动完毕，请闭眼。\n", 28, 0);
        }
    }
    
    // ============= 阶段 10: 揭示者 (Revealer) =============
    else if (current_phase == PHASE_REVEALER && pPlayer->initial_role == "揭示者") {
        
        if (cmd.find("REVEAL") == 0) {
            // 1. 检查是否已经行动过
            if (pPlayer->has_acted) {
                send(clientSock, "【错误】你在这个阶段已经行动过了。\n", 35, 0);
                return;
            }

            // 2. 解析目标编号
            int target_id = -1;
            try {
                size_t space_pos = cmd.find(' ');
                if (space_pos != std::string::npos) {
                    target_id = std::stoi(cmd.substr(space_pos + 1)) - 1;
                }
            } catch (...) {
                send(clientSock, "【错误】指令格式：REVEAL [编号]\n", 32, 0);
                return;
            }

            // 3. 验证目标有效性
            if (target_id < 0 || target_id >= (int)players.size()) {
                send(clientSock, "【错误】无效的目标编号。\n", 27, 0);
                return;
            }

            // 4. 执行揭示逻辑
            // 注意：揭示者看的是玩家当前的身份 (current_role)
            std::string target_role = players[target_id].current_role;
            pPlayer->has_acted = true;

            // 5. 判定处理：如果不是好人，不公示，仅私聊通知
            if (!is_good_team(target_role)) {
                std::string private_msg = "【揭示者】" + std::to_string(target_id + 1) + 
                                        " 号玩家的身份是 " + target_role + "。因为它不是好人，身份已被翻回，不向全场公示。\n";
                send(clientSock, private_msg.c_str(), (int)private_msg.length(), 0);
            } 
            else {
                // 如果是好人阵营，做上标记，天亮再通知
                players[target_id].is_revealed = true; // 标记该玩家已被揭示，天亮时会统一展示
                std::string private_msg = "揭示者翻开了 " + std::to_string(target_id + 1) + 
                                        " 号玩家的牌，身份是: " + target_role + "\n";
                
                send(clientSock, private_msg.c_str(), (int)private_msg.length(), 0);
                
            }

            send(clientSock, "【提示】行动完毕，请闭眼。\n", 28, 0);
        }
    }
}

#undef send
