#pragma once

#include "room.h"

#include <string>
#include <vector>

bool sendLine(SOCKET socket, const std::string& message);
bool sendLine(Room& room, SOCKET socket, const std::string& message);
void broadcast(Room& room, const std::string& message);
void broadcastRoomStatus(Room& room);
void send_to_role(Room& room, const std::string& roleTarget, const std::string& message);
void send_to_doppel_role(Room& room, const std::string& roleTarget, const std::string& message);

void broadcastPhase(Room& room, const std::string& phase, const std::string& step,
                    const std::string& prompt);
void sendAction(Room& room, SOCKET socket, const std::string& action,
                const std::string& prompt);

std::string escapeProtocolField(const std::string& value);
std::string unescapeProtocolField(const std::string& value);
std::vector<std::string> splitProtocol(const std::string& value, char delimiter = '|');
