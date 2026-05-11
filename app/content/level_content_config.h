#pragma once

struct LevelInfo {
    const char* title;
    const char* beatmap_path;
};

namespace content_config {

inline constexpr int LEVEL_COUNT = 3;
inline constexpr int DIFFICULTY_COUNT = 3;

extern const LevelInfo LEVELS[LEVEL_COUNT];
extern const char* const LEVEL_KEYS[LEVEL_COUNT];
extern const char* const DIFFICULTY_NAMES[DIFFICULTY_COUNT];
extern const char* const DIFFICULTY_KEYS[DIFFICULTY_COUNT];

}  // namespace content_config
