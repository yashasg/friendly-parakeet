#pragma once

#include <entt/entt.hpp>

struct ButtonPressEvent;
struct GoEvent;

void level_select_handle_go(entt::registry& reg, const GoEvent& evt);
void level_select_handle_press(entt::registry& reg, const ButtonPressEvent& evt);
void sync_level_select_button_layout(entt::registry& reg);
