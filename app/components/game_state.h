#pragma once

#include <cstdint>
#include <string>
#include <entt/entt.hpp>

enum class GamePhase : uint8_t {
    Title        = 0,
    LevelSelect  = 1,
    Playing      = 2,
    Paused       = 3,
    GameOver     = 4,
    SongComplete = 5
};

// Typed phase-mask enum for ActiveInPhase.  Values are power-of-two so that
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
    _entt_enum_as_bitmask   // sentinel: activates entt typed operators
};

// Convert a GamePhase state value to its corresponding GamePhaseBit.
[[nodiscard]] constexpr GamePhaseBit to_phase_bit(GamePhase p) noexcept {
    return static_cast<GamePhaseBit>(uint8_t{1} << static_cast<uint8_t>(p));
}

enum class EndScreenChoice : uint8_t { None = 0, Restart = 1, LevelSelect = 2, MainMenu = 3 };

struct GameState {
    GamePhase phase            = GamePhase::Title;
    GamePhase previous_phase   = GamePhase::Title;
    float     phase_timer      = 0.0f;
    bool      transition_pending = false;
    GamePhase next_phase       = GamePhase::Title;
    float     transition_alpha = 0.0f;
    EndScreenChoice end_choice = EndScreenChoice::None;
};

struct LevelInfo {
    const char* title;
    const char* beatmap_path;
};

struct LevelSelectState {
    static constexpr int LEVEL_COUNT = 3;
    static constexpr int DIFFICULTY_COUNT = 3;
    static constexpr LevelInfo LEVELS[LEVEL_COUNT] = {
        {"STOMPER",            "content/beatmaps/1_stomper_beatmap.json"},
        {"DRAMA",              "content/beatmaps/2_drama_beatmap.json"},
        {"MENTAL CORRUPTION",  "content/beatmaps/3_mental_corruption_beatmap.json"},
    };
    static constexpr const char* DIFFICULTY_NAMES[DIFFICULTY_COUNT] = {"EASY", "MEDIUM", "HARD"};
    static constexpr const char* DIFFICULTY_KEYS[DIFFICULTY_COUNT]  = {"easy", "medium", "hard"};

    int selected_level      = 0;
    int selected_difficulty  = 1;  // default medium
    bool confirmed          = false;
};
