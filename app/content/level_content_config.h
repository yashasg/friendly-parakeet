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

extern const LevelInfo LEVELS[LEVEL_COUNT];
extern const char* const LEVEL_KEYS[LEVEL_COUNT];
extern const char* const DIFFICULTY_NAMES[DIFFICULTY_COUNT];
extern const char* const DIFFICULTY_KEYS[DIFFICULTY_COUNT];

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
