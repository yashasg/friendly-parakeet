#pragma once

#include <entt/entt.hpp>

// Level-select dynamic spawner (issue #1296).
//
// Emits the per-level card entities and per-card difficulty button
// entities. Runs alongside the codegen-emitted
// `spawn_level_select_screen` (which only emits HeaderText + StartButton)
// by `screen_lifecycle_system` when the LevelSelect phase is entered.
//
// All entities carry `LevelSelectScreenTag` so the codegen-emitted
// `despawn_level_select_screen(reg)` destroys them on phase exit — no
// separate despawn function needed.
//
// Card + difficulty-button geometry constants (`kCard*` / `kDiff*`) live in
// `level_select_dynamic_spawn_system.cpp` as the single source of truth;
// see `app/systems/ui_render_system.cpp` for the matching render block.
void level_select_dynamic_spawn(entt::registry& reg);

// Combined spawn entry-point used by `screen_lifecycle_system` — calls
// the codegen-emitted `spawn_level_select_screen` (static layout) then
// `level_select_dynamic_spawn` (dynamic cards + difficulty buttons).
void spawn_level_select_screen_full(entt::registry& reg);
