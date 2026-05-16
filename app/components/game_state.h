#pragma once

#include <cstdint>
#include <entt/entt.hpp>

// Per Fabian's existential processing (issue #1202/#1204): the former
// `GamePhase` enum and its `GameState::phase`/`next_phase`/`transition_pending`
// mirror fields have been eradicated. Active phase and pending-transition
// requests are now expressed entirely as ctx-singleton tag presence on
// `entt::registry` (see the `GamePhase*Tag` / `NextPhase*Tag` struct sets
// below and `app/systems/game_phase_transition.h` for the API).
struct GameState {
    float phase_timer = 0.0f;
};

// ── Game phase (per-tag ctx tables; exactly one present at any time) ──
// Per Fabian's existential processing (issue #1202/#1204), each former
// GamePhase enum value gets its own zero-column table on `registry.ctx()`.
// Exactly one of these tags is present at any time; `enter_phase<...>()`
// (see `systems/game_phase_transition.h`) is the sole writer.
struct GamePhaseTitleTag        {};
struct GamePhaseLevelSelectTag  {};
struct GamePhasePlayingTag      {};
struct GamePhasePausedTag       {};
struct GamePhaseGameOverTag     {};
struct GamePhaseSongCompleteTag {};
struct GamePhaseSettingsTag     {};
struct GamePhaseTutorialTag     {};

// ── Pending phase transition (per-tag ctx tables) ──────────────────────
// Per Fabian existential processing (issue #1202/#1204), the per-frame
// "transition request" target is expressed as ctx-tag presence. UI / input
// systems call `request_phase_transition<NextPhase*Tag>()` instead of
// writing an enum field; `game_state_system` reads tag presence at the top
// of its dispatch block, performs the per-tag swap, and drains the mirror
// via `clear_next_phase_tags()`. Outside that block exactly zero
// `NextPhase*Tag` ctx slots are present; inside it, exactly one.
struct NextPhaseTitleTag        {};
struct NextPhaseLevelSelectTag  {};
struct NextPhasePlayingTag      {};
struct NextPhasePausedTag       {};
struct NextPhaseGameOverTag     {};
struct NextPhaseSongCompleteTag {};
struct NextPhaseSettingsTag     {};
struct NextPhaseTutorialTag     {};

// ── End-screen menu choice (per-choice ctx tables) ───────────
// Per Fabian's existential processing (issue #1202/#1204), each former
// EndScreenChoice value is its own zero-column table on `registry.ctx()`.
// Presence of an EndChoice* tag signals the player's selection on a Game
// Over or Song Complete screen; absence of all three is the "no choice
// yet" state (what EndScreenChoice::None represented). One transform per
// tag in `game_state_end_screen_system` consumes the choice when the
// per-phase input delay has elapsed; until then the tag persists.
struct EndChoiceRestart {};
struct EndChoiceLevelSelect {};
struct EndChoiceMainMenu {};

struct LevelSelectState {
    int selected_level      = 0;
    int selected_difficulty  = 1;  // default medium
    bool confirmed          = false;
};

// ── Game-over cause (per-cause ctx tables) ──────────────────
// Per Fabian's existential processing, each former DeathCause value is its
// own zero-column table on `registry.ctx()`. Presence of a *Death tag is the
// data; absence of all *Death tags means "no terminal cause recorded" (what
// the former DeathCause::None sentinel represented).
//
// The system that triggers the end-of-run condition emplaces the matching
// tag; the Game Over screen controller reads tag presence to surface a
// one-line, platform-neutral, colorblind-safe reason.
struct EnergyDepletedDeath {};

// ── Terminal Result Snapshot (singleton) ─────────────────────
// Captured by game_state_terminal_phase_system at the moment of song
// completion / game over.  Read by the Game Over / Song Complete screen
// controllers (and high-score haptic feedback) to surface "new best" UI.
struct TerminalResultState {
    bool    new_best      = false;
    int32_t previous_best = 0;
};
