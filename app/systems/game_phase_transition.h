#pragma once

#include <entt/entt.hpp>

#include "../components/game_state.h"

void enter_phase(entt::registry& reg, GameState& gs, GamePhase next_phase);

// Erases all eight `GamePhase*Tag` ctx slots and emplaces the single tag
// matching `phase`. Public so `reset_ctx_singleton<GameState>(reg, ...)`
// callers (game_loop bootstrap, test setup) can prime the mirror at the
// same moment they install the GameState ctx singleton. `enter_phase()`
// invokes this internally on every transition.
void sync_game_phase_tags(entt::registry& reg, GamePhase phase);
