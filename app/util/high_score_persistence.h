#pragma once

#include "../components/high_score.h"
#include <filesystem>

namespace high_score {

// Load high scores from JSON file.
// Distinguishes missing/corrupt/path errors via structured status.
persistence::Result load_high_scores(HighScoreState& state, const std::filesystem::path& path);

// Save high scores to JSON file.
// Distinguishes path/open/write failures via structured status.
persistence::Result save_high_scores(const HighScoreState& state, const std::filesystem::path& path);

// Get platform-specific high scores directory (same as settings dir).
std::filesystem::path get_high_scores_dir();

// Resolve full high scores file path.
// Returns structured failure and clears out_path when resolution fails.
persistence::Result get_high_scores_file_path(
    std::filesystem::path& out_path,
    const std::filesystem::path& root_override = {});

// Update the stored score for state.current_key_hash if new_score is strictly higher.
// Negative new_score is clamped to 0. No-op if current_key_hash is zero.
void update_if_higher(HighScoreState& state, int32_t new_score);

// Build "song_id|difficulty" into caller-supplied buf (no heap allocation).
// Returns snprintf-style char count (excluding NUL). cap should be HighScoreState::KEY_CAP.
int32_t make_key_str(char* buf, int32_t cap, const char* song_id, const char* difficulty);

// Compute the FNV-1a 32-bit hash for a song+difficulty key (no heap allocation).
entt::hashed_string::hash_type make_key_hash(const char* song_id, const char* difficulty);

// Returns the stored score for key, or 0 if not found.
int32_t get_score(const HighScoreState& state, const char* key);

// Returns the stored score for a hash -- used on the session current-key path.
int32_t get_score_by_hash(const HighScoreState& state, entt::hashed_string::hash_type hash);

// Insert or update entry by string key. Silently drops if table is full.
void set_score(HighScoreState& state, const char* key, int32_t score);

// Update an existing entry's score by hash. No-op if hash is not found.
void set_score_by_hash(HighScoreState& state, entt::hashed_string::hash_type hash, int32_t score);

// Ensure an entry exists for key with score 0 if not already present.
void ensure_entry(HighScoreState& state, const char* key);

// Returns the stored score for the current session key, or 0 if no session active.
int32_t get_current_high_score(const HighScoreState& state);

} // namespace high_score
