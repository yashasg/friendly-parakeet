#pragma once

#include <cstdint>
#include <cstring>

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

[[nodiscard]] constexpr bool is_end_screen_phase(GamePhase phase) noexcept {
    return phase == GamePhase::GameOver || phase == GamePhase::SongComplete;
}

[[nodiscard]] constexpr bool is_playing_phase(GamePhase phase) noexcept {
    return phase == GamePhase::Playing;
}

[[nodiscard]] constexpr bool is_menu_phase(GamePhase phase) noexcept {
    return phase == GamePhase::Title ||
           phase == GamePhase::LevelSelect ||
           phase == GamePhase::Paused ||
           is_end_screen_phase(phase) ||
           phase == GamePhase::Settings ||
           phase == GamePhase::Tutorial;
}

namespace game_phase_timing {
constexpr float END_SCREEN_INPUT_DELAY_SEC = 0.4f;
constexpr float SONG_COMPLETE_TRANSITION_DELAY_SEC = 0.5f;
constexpr float LEVEL_SELECT_INPUT_DELAY_SEC = 0.05f;
constexpr float LEVEL_SELECT_CONFIRM_DELAY_SEC = 0.2f;
}  // namespace game_phase_timing

struct GameState {
    GamePhase phase            = GamePhase::Title;
    GamePhase previous_phase   = GamePhase::Title;
    float     phase_timer      = 0.0f;
    bool      transition_pending = false;
    GamePhase next_phase       = GamePhase::Title;
    EndScreenChoice end_choice = EndScreenChoice::None;
};

inline void enter_phase(GameState& gs, GamePhase next_phase) {
    gs.previous_phase = gs.phase;
    gs.phase = next_phase;
    gs.phase_timer = 0.0f;
}

inline void request_phase_transition(GameState& gs, GamePhase target_phase) {
    gs.transition_pending = true;
    gs.next_phase = target_phase;
}

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

[[nodiscard]] constexpr int clamp_level_index(int level_index) noexcept {
    if (level_index < 0) return 0;
    if (level_index >= LevelSelectState::LEVEL_COUNT) return LevelSelectState::LEVEL_COUNT - 1;
    return level_index;
}

[[nodiscard]] constexpr int clamp_difficulty_index(int difficulty_index) noexcept {
    if (difficulty_index < 0) return 0;
    if (difficulty_index >= LevelSelectState::DIFFICULTY_COUNT) {
        return LevelSelectState::DIFFICULTY_COUNT - 1;
    }
    return difficulty_index;
}

[[nodiscard]] inline const LevelInfo& selected_level_info(const LevelSelectState& state) noexcept {
    return LevelSelectState::LEVELS[clamp_level_index(state.selected_level)];
}

[[nodiscard]] inline const char* selected_difficulty_name(const LevelSelectState& state) noexcept {
    return LevelSelectState::DIFFICULTY_NAMES[clamp_difficulty_index(state.selected_difficulty)];
}

[[nodiscard]] inline const char* selected_difficulty_key(const LevelSelectState& state) noexcept {
    return LevelSelectState::DIFFICULTY_KEYS[clamp_difficulty_index(state.selected_difficulty)];
}

[[nodiscard]] inline int find_difficulty_index(const char* difficulty_key) noexcept {
    if (!difficulty_key) return -1;
    for (int i = 0; i < LevelSelectState::DIFFICULTY_COUNT; ++i) {
        if (std::strcmp(LevelSelectState::DIFFICULTY_KEYS[i], difficulty_key) == 0) {
            return i;
        }
    }
    return -1;
}

[[nodiscard]] inline bool is_supported_difficulty_key(const char* difficulty_key) noexcept {
    return find_difficulty_index(difficulty_key) >= 0;
}
