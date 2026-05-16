#pragma once

#include <entt/entt.hpp>

// Game Over dynamic-text binder (issue #1293, refs #1287, #1193 OoS-A).
//
// Writes per-frame score / high-score / new-best / previous-best / death-
// reason text into the dynamic-text label slots spawned by
// `spawn_game_over_screen()` (codegen, see
// `app/ui/generated/game_over_screen.cpp`). The slot entities are identified
// by their canonical (x, y) position baked from
// `content/ui/screens/game_over.rgl`; the bind table is a per-slot row of
// (x, y, write fn) — no `switch` over a discriminator (Fabian Principle 1).
//
// The two reason slots (`ReasonSlot` at y=685 and `ReasonSlotNewBest` at
// y=742) mirror the legacy controller's positional shift: when
// `TerminalResultState.new_best` is set, the "ENERGY DEPLETED" reason moves
// down to make room for the "NEW BEST!" + "PREV N" lines.
//
// Runs after `screen_lifecycle_system` so the GameOver entity set is
// guaranteed to exist whenever `GamePhaseGameOverTag` is the active phase;
// trivially no-ops on every other phase (the view is empty).
void game_over_scoreboard_bind_system(entt::registry& reg);
