#pragma once

#include <entt/entt.hpp>

struct GoUpEvent;
struct GoDownEvent;
struct GoLeftEvent;
struct GoRightEvent;
struct MenuConfirmEvent;
struct MenuSelectLevelEvent;
struct MenuSelectDiffEvent;

// Per-event handlers (#1277/#1279): the event type IS the choice; no
// enum-typed discriminator at the consumer call site.
void level_select_handle_go_up    (entt::registry& reg, const GoUpEvent&);
void level_select_handle_go_down  (entt::registry& reg, const GoDownEvent&);
void level_select_handle_go_left  (entt::registry& reg, const GoLeftEvent&);
void level_select_handle_go_right (entt::registry& reg, const GoRightEvent&);

void level_select_handle_confirm     (entt::registry& reg, const MenuConfirmEvent&);
void level_select_handle_select_level(entt::registry& reg, const MenuSelectLevelEvent& evt);
void level_select_handle_select_diff (entt::registry& reg, const MenuSelectDiffEvent&  evt);
