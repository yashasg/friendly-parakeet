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

// Typed phase-mask enum for phase mask use-cases. Values are power-of-two so that
// multiple phases can be OR-combined into a single mask.  The
// _entt_enum_as_bitmask sentinel enables entt's typed |/&/^ operators,
// replacing the previous hand-rolled uint8_t shift helpers.
//
// GamePhase (sequential) stays as the canonical state-machine value stored in
// GameState; this separate enum exists purely for "which phases is this entity
// active in" semantics.
enum class GamePhaseBit : uint8_t {
    Title        = 1 << 0,  // 0x01
    LevelSelect  = 1 << 1,  // 0x02
    Playing      = 1 << 2,  // 0x04
    Paused       = 1 << 3,  // 0x08
    GameOver     = 1 << 4,  // 0x10
    SongComplete = 1 << 5,  // 0x20
    Settings     = 1 << 6,  // 0x40
    Tutorial     = 1 << 7,  // 0x80 (fills uint8_t; combine with caution)
    _entt_enum_as_bitmask   // sentinel: activates entt typed operators
};

// Convert a GamePhase state value to its corresponding GamePhaseBit.
[[nodiscard]] constexpr GamePhaseBit to_phase_bit(GamePhase p) noexcept {
    return static_cast<GamePhaseBit>(uint8_t{1} << static_cast<uint8_t>(p));
}

enum class EndScreenChoice : uint8_t { None = 0, Restart = 1, LevelSelect = 2, MainMenu = 3 };

struct GameState {
    GamePhase phase            = GamePhase::Title;
    float     phase_timer      = 0.0f;
    bool      transition_pending = false;
    GamePhase next_phase       = GamePhase::Title;
    EndScreenChoice end_choice = EndScreenChoice::None;
};

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
