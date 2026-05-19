#pragma once

#include <cstdint>
#include <string>
#include <entt/entt.hpp>

#include "../systems/persistence_policy_system.h"

// Constants for the durable high-score table. The struct itself carries no
// instance state — entries live as `HighScoreEntry` rows in the registry,
// one entity per stored score (Fabian Principle 3 / issue #1560 — the prior
// `std::array<Entry, MAX_ENTRIES> entries + int32_t entry_count` shape was
// a 1NF array column on a god-class component, structurally identical to
// the `ObstacleChildren` array column eradicated in #1554).
//
// MAX_ENTRIES = LEVEL_COUNT (3) x DIFFICULTY_COUNT (3) survives as the
// runtime overflow guard in set_score / ensure_entry / high_score_state_from_json.
struct HighScoreState {
    static constexpr int32_t MAX_ENTRIES = 9;
    static constexpr int32_t KEY_CAP     = 32; // max stored key bytes including NUL
};

// Row component: one entity per stored high score. The printable `key` is
// retained so JSON persistence preserves the on-disk schema bit-for-bit;
// `key_hash` is cached at insert time so the hot `get_current_high_score`
// lookup path doesn't rehash per row.
struct HighScoreEntry {
    char                           key[HighScoreState::KEY_CAP]{};
    entt::hashed_string::hash_type key_hash{0};
    int32_t                        score{0};
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
