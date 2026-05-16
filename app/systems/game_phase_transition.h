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

// Pending-transition tag mirror (issue #1202 / PR C). `sync_next_phase_tags`
// erases all eight `NextPhase*Tag` ctx slots and emplaces the single tag
// matching `phase`; `clear_next_phase_tags` erases all eight. The pair is
// called by `game_state_system`'s transition dispatch — sync at the top,
// clear at the bottom — so each scheduled transition produces exactly one
// pulse of "which phase was requested". This is the existential-processing
// substrate for replacing the former `switch (gs.next_phase)` with per-tag
// transforms (one if-block per phase).
void sync_next_phase_tags(entt::registry& reg, GamePhase phase);
void clear_next_phase_tags(entt::registry& reg);
