#pragma once

#include <cstdint>
#include <entt/entt.hpp>

// Phase-mirror, end-screen-choice, terminal-cause, and level-select-
// confirmation tags formerly lived in this header. Per the single-tags-
// header convention (`.squad/decisions.md` § "DoD source-text grounding
// (Fabian)" Principle: tags single-header) and Fabian drift issue #1276,
// they now live in `app/tags/tags.h` under the "Game phase mirrors",
// "Pending phase transition", "End-screen menu choice", "Game-over cause",
// and "Level-select confirmation latch" sections. The include below
// keeps existing consumers (which expect these tags via `game_state.h`)
// resolving.
#include "tags/tags.h"

// Per Fabian's existential processing (issue #1202/#1204): the former
// `GamePhase` enum and its `GameState::phase`/`next_phase`/`transition_pending`
// mirror fields have been eradicated. Active phase and pending-transition
// requests are now expressed entirely as ctx-singleton tag presence on
// `entt::registry` (see the `GamePhase*Tag` / `NextPhase*Tag` struct sets
// in `app/tags/tags.h` and `app/systems/game_phase_transition.h` for the API).
struct GameState {
    float phase_timer = 0.0f;
};

struct LevelSelectState {
    int selected_level      = 0;
    int selected_difficulty  = 1;  // default medium
};

// ── Terminal Result Snapshot (singleton) ─────────────────────
// Captured by game_state_terminal_phase_system at the moment of song
// completion / game over.  Read by the Game Over / Song Complete screen
// controllers (and high-score haptic feedback) to surface "new best" UI.
struct TerminalResultState {
    bool    new_best      = false;
    int32_t previous_best = 0;
};
