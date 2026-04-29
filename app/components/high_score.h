#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <entt/entt.hpp>

#include "../util/persistence_policy.h"

// Compact fixed-size high-score table.
// Max entries: LEVEL_COUNT x DIFFICULTY_COUNT = 9.
// Flat array storage -- no heap nodes, O(N) linear scan (faster than
// std::map for N <= 9 due to cache locality).
struct HighScoreState {
    static constexpr int32_t MAX_ENTRIES = 9;
    static constexpr int32_t KEY_CAP     = 32; // fits "song_id|difficulty\0"

    struct Entry {
        char    key[KEY_CAP]{};
        int32_t score{0};
    };

    std::array<Entry, MAX_ENTRIES> entries{};
    int32_t entry_count{0};

    // FNV-1a 32-bit hash of the current session's "song_id|difficulty" key.
    // Set once per session via high_score::make_key_hash(); zero means no active session.
    // Using a hash avoids a heap std::string allocation on the session-start path.
    // Collision risk: negligible across MAX_ENTRIES=9 keys (e.g. "1_stomper|easy").
    entt::hashed_string::hash_type current_key_hash{0};
};

struct HighScorePersistence {
    std::string path;
    persistence::Result last_load{};
    persistence::Result last_save{};
    bool dirty{false};
};
