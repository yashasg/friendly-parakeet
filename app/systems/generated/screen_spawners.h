#pragma once

#include <entt/entt.hpp>

// Forward declarations of the per-screen entity-spawner functions emitted by
// `tools/rguilayout/codegen.py` (one pair per `.rgl` in
// `content/ui/screens/`). Per #1193: each call creates one entity per
// control row in its `.rgl`, carrying atomic `UiPosition` / `UiBounds` /
// `UiLabel` (+ `UiAction*Tag` for buttons) plus a per-screen tag (e.g.
// `PausedScreenTag`) and a per-kind tag (`UiLabelTag` / `UiButtonTag` /
// `UiDummyRecTag`) — see `app/components/ui.h` and `app/tags/tags.h`.
//
// `despawn_<screen>_screen` queries `view<<ScreenTag>>()` and destroys every
// matched entity; the per-screen tag IS the membership.
//
// Wired by `screen_lifecycle_system` (`app/systems/screen_lifecycle_system.h`),
// which keeps the entity set in sync with the active `GamePhase*Tag` ctx
// mirror seeded by `enter_phase<...>()`.

void spawn_title_screen(entt::registry& reg);
void despawn_title_screen(entt::registry& reg);

void spawn_level_select_screen(entt::registry& reg);
void despawn_level_select_screen(entt::registry& reg);

void spawn_tutorial_screen(entt::registry& reg);
void despawn_tutorial_screen(entt::registry& reg);

void spawn_gameplay_screen(entt::registry& reg);
void despawn_gameplay_screen(entt::registry& reg);

void spawn_paused_screen(entt::registry& reg);
void despawn_paused_screen(entt::registry& reg);

void spawn_game_over_screen(entt::registry& reg);
void despawn_game_over_screen(entt::registry& reg);

void spawn_song_complete_screen(entt::registry& reg);
void despawn_song_complete_screen(entt::registry& reg);

void spawn_settings_screen(entt::registry& reg);
void despawn_settings_screen(entt::registry& reg);
