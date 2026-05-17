#pragma once

struct LevelInfo {
    const char* title;
    const char* beatmap_path;
};

namespace content_config {

inline constexpr int LEVEL_COUNT = 3;
inline constexpr int DIFFICULTY_COUNT = 3;
inline constexpr int DEFAULT_LEVEL_INDEX = 0;
inline constexpr int DEFAULT_DIFFICULTY_INDEX = 1;

inline constexpr LevelInfo LEVELS[LEVEL_COUNT] = {
    {"STOMPER", "content/beatmaps/1_stomper_beatmap.json"},
    {"DRAMA", "content/beatmaps/2_drama_beatmap.json"},
    {"MENTAL CORRUPTION", "content/beatmaps/3_mental_corruption_beatmap.json"},
};

inline constexpr const char* const LEVEL_KEYS[LEVEL_COUNT] = {
    "1_stomper", "2_drama", "3_mental_corruption",
};

inline constexpr const char* const DIFFICULTY_NAMES[DIFFICULTY_COUNT] = {
    "EASY", "MEDIUM", "HARD",
};

inline constexpr const char* const DIFFICULTY_KEYS[DIFFICULTY_COUNT] = {
    "easy", "medium", "hard",
};

[[nodiscard]] constexpr bool is_valid_level_index(int index) noexcept {
    return index >= 0 && index < LEVEL_COUNT;
}

[[nodiscard]] constexpr bool is_valid_difficulty_index(int index) noexcept {
    return index >= 0 && index < DIFFICULTY_COUNT;
}

[[nodiscard]] constexpr int level_index_or_default(int index) noexcept {
    return is_valid_level_index(index) ? index : DEFAULT_LEVEL_INDEX;
}

[[nodiscard]] constexpr int difficulty_index_or_default(int index) noexcept {
    return is_valid_difficulty_index(index) ? index : DEFAULT_DIFFICULTY_INDEX;
}

}  // namespace content_config
