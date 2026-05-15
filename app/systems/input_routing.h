#pragma once

#include <entt/entt.hpp>

struct ButtonPressEvent;
struct GoEvent;

// Semantic listeners: events -> game/player state.
void game_state_handle_go(entt::registry& reg, const GoEvent& evt);
void game_state_handle_press(entt::registry& reg, const ButtonPressEvent& evt);
void player_input_handle_go(entt::registry& reg, const GoEvent& evt);
void player_input_handle_press(entt::registry& reg, const ButtonPressEvent& evt);

// Dispatcher wiring for input and semantic event listeners.
void wire_input_dispatcher(entt::registry& reg);
void unwire_input_dispatcher(entt::registry& reg);
