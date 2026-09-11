#pragma once

#include "game_types.h"
#include "room_manager.h"

void handle_client(SOCKET clientSocket, RoomManager& roomManager);
