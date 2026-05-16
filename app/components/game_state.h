#pragma once

#include <cstdint>
#include <entt/entt.hpp>

enum class GamePhase : uint8_t {
    Title        = 0,
    LevelSelect  = 1,
    Playing      = 2,
    Paused       = 3,
    GameOver     = 4,
    SongComplete = 5,
    Settings     = 6,
    Tutorial     = 7
};

struct GameState {
    GamePhase phase            = GamePhase::Title;
    float     phase_timer      = 0.0f;
    bool      transition_pending = false;
    GamePhase next_phase       = GamePhase::Title;
};

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
