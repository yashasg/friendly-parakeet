#pragma once

#include <entt/entt.hpp>

struct GoEvent;
struct MenuConfirmEvent;
struct MenuSelectLevelEvent;
struct MenuSelectDiffEvent;

void level_select_handle_go(entt::registry& reg, const GoEvent& evt);

// Per-event handlers (#1277): the event type IS the choice; no enum-typed
// discriminator at the consumer call site.
void level_select_handle_confirm     (entt::registry& reg, const MenuConfirmEvent&);
void level_select_handle_select_level(entt::registry& reg, const MenuSelectLevelEvent& evt);
void level_select_handle_select_diff (entt::registry& reg, const MenuSelectDiffEvent&  evt);
