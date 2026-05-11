#include "level_content_config.h"

namespace content_config {

const LevelInfo LEVELS[LEVEL_COUNT] = {
    {"STOMPER", "content/beatmaps/1_stomper_beatmap.json"},
    {"DRAMA", "content/beatmaps/2_drama_beatmap.json"},
    {"MENTAL CORRUPTION", "content/beatmaps/3_mental_corruption_beatmap.json"},
};

const char* const LEVEL_KEYS[LEVEL_COUNT] = {"1_stomper", "2_drama", "3_mental_corruption"};
const char* const DIFFICULTY_NAMES[DIFFICULTY_COUNT] = {"EASY", "MEDIUM", "HARD"};
const char* const DIFFICULTY_KEYS[DIFFICULTY_COUNT] = {"easy", "medium", "hard"};

}  // namespace content_config
