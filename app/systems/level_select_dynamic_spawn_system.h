#pragma once

#include <entt/entt.hpp>

// Level-select dynamic spawner (issue #1296).
//
// Emits the per-level card entities and per-card difficulty button
// entities that the legacy `level_select_screen_controller.cpp` painted
// imperatively. Run alongside the codegen-emitted
// `spawn_level_select_screen` (which only emits HeaderText + StartButton)
// by `screen_lifecycle_system` when the LevelSelect phase is entered.
//
// All entities carry `LevelSelectScreenTag` so the codegen-emitted
// `despawn_level_select_screen(reg)` destroys them on phase exit — no
// separate despawn function needed.
//
// Card geometry mirrors the legacy controller constants (CARD_X=110,
// CARD_W=500, CARD_H=200, CARD_GAP=40, CARD_START_Y=200) and the
// difficulty-row constants (DIFF_BTN_W=120, DIFF_BTN_H=50, DIFF_GAP=30,
// DIFF_START_X=160, DIFF_Y_OFFSET=120). See `app/systems/ui_render_system.cpp`
// for the matching render constants.
void level_select_dynamic_spawn(entt::registry& reg);

// Combined spawn entry-point used by `screen_lifecycle_system` — calls
// the codegen-emitted `spawn_level_select_screen` (static layout) then
// `level_select_dynamic_spawn` (dynamic cards + difficulty buttons).
void spawn_level_select_screen_full(entt::registry& reg);
