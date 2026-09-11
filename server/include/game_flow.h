#pragma once

#include "room.h"

#include <memory>

void print_final_identities(Room& room);
void game_flow_controller(std::shared_ptr<Room> room);
void reset_game_state(Room& room);
