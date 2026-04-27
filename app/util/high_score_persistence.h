#pragma once

#include "../components/high_score.h"
#include <filesystem>

namespace high_score {

// Load high scores from JSON file.
// If file doesn't exist or parsing fails, returns false and leaves state unchanged.
bool load_high_scores(HighScoreState& state, const std::filesystem::path& path);

// Save high scores to JSON file.
// Returns false if write fails.
bool save_high_scores(const HighScoreState& state, const std::filesystem::path& path);

// Get platform-specific high scores directory (same as settings dir).
std::filesystem::path get_high_scores_dir();

// Get full high scores file path (dir/high_scores.json)
std::filesystem::path get_high_scores_file_path();

// Update the stored score for state.current_key if new_score is strictly higher.
// Negative new_score is clamped to 0. No-op if current_key is empty.
void update_if_higher(HighScoreState& state, int32_t new_score);

} // namespace high_score
