#pragma once

#include <entt/entt.hpp>

// Song Complete dynamic-text binder (issue #1292, refs #1287, #1193 OoS-A).
//
// Writes per-frame score / high-score / NEW BEST / stats / energy text into
// the dynamic-text label slots spawned by `spawn_song_complete_screen()`
// (codegen, see `app/systems/generated/song_complete_screen.cpp`). The slot
// entities are identified by their canonical (x, y) position baked from
// `content/ui/screens/song_complete.rgl`; the bind table is a per-slot row
// of (x, y, write fn) — no `switch` over a discriminator.
//
// Runs after `screen_lifecycle_system` so the SongComplete entity set is
// guaranteed to exist whenever `GamePhaseSongCompleteTag` is the active
// phase; trivially no-ops on every other phase (the view is empty).
void song_complete_scoreboard_bind_system(entt::registry& reg);
