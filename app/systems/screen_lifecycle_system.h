#pragma once

#include <entt/entt.hpp>

// Keeps per-screen UI entity sets in sync with the active `GamePhase*Tag`
// ctx mirror (issue #1287, refs #1193 OoS-A; legacy controllers deleted
// in #1308).
//
// Per Fabian's existential processing, the membership "which entities
// belong to which screen" lives in tag presence — not in an enum field,
// not in a god-struct. Each `.rgl` produces a `spawn_<screen>_screen()` +
// `despawn_<screen>_screen()` pair (codegen, see
// `app/systems/generated/screen_spawners.h`). This system converges the entity
// set toward whatever `GamePhase*Tag` is currently in `reg.ctx()`: if the
// matching screen-tag has no entities and the phase tag is present, spawn;
// if the phase tag is absent and entities still exist, despawn.
void screen_lifecycle_system(entt::registry& reg);
