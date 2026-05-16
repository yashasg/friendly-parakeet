#pragma once

#include <entt/entt.hpp>

struct GoEvent;
struct MenuPressEvent;
struct ShapePressCircleEvent;
struct ShapePressSquareEvent;
struct ShapePressTriangleEvent;

// Semantic listeners: events -> game/player state.
// Per issue #1202/#1204, the press-event handler tree is identity-encoded
// in the event type — no `switch (kind)` at consumers; the type IS the choice.
void game_state_handle_go(entt::registry& reg, const GoEvent& evt);
void game_state_handle_press_menu(entt::registry& reg, const MenuPressEvent& evt);

void player_input_handle_go(entt::registry& reg, const GoEvent& evt);
void player_input_handle_press_circle  (entt::registry& reg, const ShapePressCircleEvent&   evt);
void player_input_handle_press_square  (entt::registry& reg, const ShapePressSquareEvent&   evt);
void player_input_handle_press_triangle(entt::registry& reg, const ShapePressTriangleEvent& evt);

// Dispatcher wiring for input and semantic event listeners.
void wire_input_dispatcher(entt::registry& reg);
void unwire_input_dispatcher(entt::registry& reg);
