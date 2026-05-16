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

// ── Game phase (per-tag ctx tables; exactly one present at any time) ──
// Per Fabian's existential processing (issue #1202/#1204), each former
// GamePhase enum value gets its own zero-column table on `registry.ctx()`.
// Presence of exactly one of these tags mirrors `GameState::phase` 1:1.
// Both representations are maintained in lockstep by `enter_phase()` and
// the initial GameState ctx emplace; the enum-typed field is retained
// during the staged migration so existing consumers continue to compile.
//
// PR F (issue #1202) finished migrating every `if (gs.phase == X)` consumer
// in `app/` onto `reg.ctx().contains<GamePhase*Tag>()`. The only remaining
// `gs.phase` reads in `app/` are the two helper switches inside
// `game_phase_transition.cpp` that translate the enum into the tag mirror
// and the `GamePhase phase` function parameter on
// `game_state_enter_terminal_phase()`; all three die with the enum itself
// in PR G, alongside `GameState::phase` and `GameState::next_phase`.
//
// Maintenance invariant: exactly one `GamePhase*Tag` ctx slot is present
// after any `enter_phase()` call. The current entry helper erases all
// eight before emplacing the matching tag; tests pin the invariant.
struct GamePhaseTitleTag        {};
struct GamePhaseLevelSelectTag  {};
struct GamePhasePlayingTag      {};
struct GamePhasePausedTag       {};
struct GamePhaseGameOverTag     {};
struct GamePhaseSongCompleteTag {};
struct GamePhaseSettingsTag     {};
struct GamePhaseTutorialTag     {};

// ── Pending phase transition (per-tag ctx tables) ──────────────────────
// Per Fabian existential processing (issue #1202/#1204, PR C), the
// per-frame "transition request" target is mirrored 1:1 into per-tag ctx
// slots so the transition dispatch in `game_state_system` can run as
// per-tag transforms instead of a `switch (gs.next_phase)`. The mirror
// is populated from `gs.next_phase` at the top of the dispatch block via
// `sync_next_phase_tags()` and cleared at the bottom via
// `clear_next_phase_tags()`; outside of that block exactly zero
// `NextPhase*Tag` ctx slots are present. The enum-typed field is retained
// during the staged migration (PRs C–F) and deleted with the enum itself
// in PR G.
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
