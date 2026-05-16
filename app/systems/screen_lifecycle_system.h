#pragma once

#include <entt/entt.hpp>

// Keeps per-screen UI entity sets in sync with the active `GamePhase*Tag`
// ctx mirror (issue #1287, refs #1193 OoS-A).
//
// Per Fabian's existential processing, the membership "which entities
// belong to which screen" lives in tag presence — not in an enum field,
// not in a god-struct. Each `.rgl` produces a `spawn_<screen>_screen()` +
// `despawn_<screen>_screen()` pair (codegen, see
// `app/ui/generated/screen_spawners.h`). This system converges the entity
// set toward whatever `GamePhase*Tag` is currently in `reg.ctx()`: if the
// matching screen-tag has no entities and the phase tag is present, spawn;
// if the phase tag is absent and entities still exist, despawn.
//
// Migration boundary: screens are migrated from
// `app/ui/screen_controllers/*` one at a time. Until every screen migrates,
// only the screens explicitly handled here participate; un-migrated screens
// continue to render via their legacy controller and stay invisible to
// this system. See #1287 for the per-screen sub-issue ladder.
//
// Migrated so far: Paused (#1290 pilot), Tutorial (#1291), Song Complete (#1292),
// Game Over (#1293).
void screen_lifecycle_system(entt::registry& reg);
