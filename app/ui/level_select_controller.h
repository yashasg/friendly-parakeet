#pragma once

#include <entt/entt.hpp>

struct GoEvent;
struct MenuPressEvent;

void level_select_handle_go(entt::registry& reg, const GoEvent& evt);
void level_select_handle_press_menu(entt::registry& reg, const MenuPressEvent& evt);
