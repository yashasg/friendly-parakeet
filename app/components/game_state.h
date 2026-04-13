#pragma once

#include <cstdint>
#include <string>

enum class GamePhase : uint8_t {
    Title        = 0,
    LevelSelect  = 1,
    Playing      = 2,
    Paused       = 3,
    GameOver     = 4,
    SongComplete = 5
};

enum class EndScreenChoice : uint8_t { None = 0, LevelSelect = 1, MainMenu = 2 };

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
    static constexpr LevelInfo LEVELS[LEVEL_COUNT] = {
        {"STOMPER",            "content/beatmaps/1_stomper_beatmap.json"},
        {"DRAMA",              "content/beatmaps/2_drama_beatmap.json"},
        {"MENTAL CORRUPTION",  "content/beatmaps/3_mental_corruption_beatmap.json"},
    };
    static constexpr const char* DIFFICULTY_NAMES[3] = {"EASY", "MEDIUM", "HARD"};
    static constexpr const char* DIFFICULTY_KEYS[3]  = {"easy", "medium", "hard"};

    int selected_level      = 0;
    int selected_difficulty  = 1;  // default medium
    bool confirmed          = false;
};
