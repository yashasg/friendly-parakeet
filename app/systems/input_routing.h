#pragma once

#include <entt/entt.hpp>

struct GoUpEvent;
struct GoDownEvent;
struct GoLeftEvent;
struct GoRightEvent;
struct MenuConfirmEvent;
struct MenuRestartEvent;
struct MenuGoLevelSelectEvent;
struct MenuGoMainMenuEvent;
struct ShapePressCircleEvent;
struct ShapePressSquareEvent;
struct ShapePressTriangleEvent;

// Semantic listeners: events -> game/player state.
// Per issues #1202/#1204/#1277/#1279, the press- and directional-event
// handler trees are identity-encoded in the event type — no
// `switch (kind)` / `if (evt.action == X)` / `if (evt.dir == X)` at
// consumers; the type IS the choice.
void game_state_handle_go_up    (entt::registry& reg, const GoUpEvent&);
void game_state_handle_go_down  (entt::registry& reg, const GoDownEvent&);
void game_state_handle_go_left  (entt::registry& reg, const GoLeftEvent&);
void game_state_handle_go_right (entt::registry& reg, const GoRightEvent&);
void game_state_handle_confirm         (entt::registry& reg, const MenuConfirmEvent&);
void game_state_handle_restart         (entt::registry& reg, const MenuRestartEvent&);
void game_state_handle_go_level_select (entt::registry& reg, const MenuGoLevelSelectEvent&);
void game_state_handle_go_main_menu    (entt::registry& reg, const MenuGoMainMenuEvent&);

void player_input_handle_go_up    (entt::registry& reg, const GoUpEvent&);
void player_input_handle_go_down  (entt::registry& reg, const GoDownEvent&);
void player_input_handle_go_left  (entt::registry& reg, const GoLeftEvent&);
void player_input_handle_go_right (entt::registry& reg, const GoRightEvent&);
void player_input_handle_press_circle  (entt::registry& reg, const ShapePressCircleEvent&   evt);
void player_input_handle_press_square  (entt::registry& reg, const ShapePressSquareEvent&   evt);
void player_input_handle_press_triangle(entt::registry& reg, const ShapePressTriangleEvent& evt);

// Dispatcher wiring for input and semantic event listeners.
void wire_input_dispatcher(entt::registry& reg);
void unwire_input_dispatcher(entt::registry& reg);
