#pragma once

#include "../components/high_score.h"
#include <filesystem>

namespace high_score {

// Load high scores from JSON file into reg's HighScoreEntry table (atomic:
// on parse failure existing rows are preserved).
// Distinguishes missing/corrupt/path errors via structured status.
persistence::Result load_high_scores(entt::registry& reg, const std::filesystem::path& path);

// Save the current HighScoreEntry rows to JSON file.
// Distinguishes path/open/write failures via structured status.
persistence::Result save_high_scores(const entt::registry& reg, const std::filesystem::path& path);

// Resolve full high scores file path.
// Returns structured failure and clears out_path when resolution fails.
persistence::Result get_high_scores_file_path(
    std::filesystem::path& out_path,
    const std::filesystem::path& root_override = {});

// Update the stored score for session.key_hash if new_score is strictly higher.
// Negative new_score is clamped to 0. Returns false when the active key is
// missing from the entry table.
bool update_if_higher(entt::registry& reg, const HighScoreSession& session, int32_t new_score);

// Build "song_id|difficulty" into caller-supplied buf (no heap allocation).
// Returns snprintf-style char count (excluding NUL), or -1 if cap is invalid
// or the key would not fit in the supplied buffer.
// cap should be HighScoreState::KEY_CAP.
int32_t make_key_str(char* buf, int32_t cap, const char* song_id, const char* difficulty);

// Compute the FNV-1a 32-bit hash for a song+difficulty key (no heap allocation).
entt::hashed_string::hash_type make_key_hash(const char* song_id, const char* difficulty);

// Returns the stored score for key, or 0 if not found.
int32_t get_score(const entt::registry& reg, const char* key);

// Returns the stored score for a hash -- used on the session current-key path.
int32_t get_score_by_hash(const entt::registry& reg, entt::hashed_string::hash_type hash);

// Insert or update entry by string key. Returns false if the table is full.
bool set_score(entt::registry& reg, const char* key, int32_t score);

// Ensure an entry exists for key with score 0 if not already present.
// Returns false if the table is full.
bool ensure_entry(entt::registry& reg, const char* key);

// Returns the stored score for the current session key, or 0 if no entry exists.
int32_t get_current_high_score(const entt::registry& reg, const HighScoreSession& session);

// Count of stored entries (rows in the HighScoreEntry table).
int32_t entry_count(const entt::registry& reg);

} // namespace high_score
