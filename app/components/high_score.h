#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <entt/entt.hpp>

#include "../systems/persistence_policy_system.h"

// Compact fixed-size high-score table (durable, persisted to disk).
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
};

// Active-session pointer into the score table (transient, not persisted).
// Split out of HighScoreState (#1194/#1203, 1NF: a NULL column whose
// meaning depends on the surrounding lifecycle belongs in its own table).
struct HighScoreSession {
    // FNV-1a 32-bit hash of the current session's "song_id|difficulty" key.
    // Set once per session via high_score::make_key_hash(); zero means no active session.
    // Using a hash avoids a heap std::string allocation on the session-start path.
    // Collision risk: negligible across MAX_ENTRIES=9 keys (e.g. "1_stomper|easy").
    entt::hashed_string::hash_type key_hash{0};
};

// Persistence I/O bookkeeping for the high-score file (path + last load/save
// results). Lives as a ctx singleton; mutated by the terminal-phase
// transition system.
//
// The former `bool dirty` column was eradicated per Fabian relational
// normalization (issue #1203): "needs save" is now expressed as the
// presence of `HighScoreDirtyTag` on `registry.ctx()` (see
// app/tags/tags.h). The terminal-phase system is the canonical writer;
// tests can emplace / erase the tag via `reg.ctx()` directly.
struct HighScorePersistence {
    std::string path;
    persistence::Result last_load{};
    persistence::Result last_save{};
};
