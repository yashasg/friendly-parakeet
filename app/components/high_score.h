#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <entt/entt.hpp>

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
    // Set once per session via make_key_hash(); zero means no active session.
    // Using a hash avoids a heap std::string allocation on the session-start path.
    // Collision risk: negligible across MAX_ENTRIES=9 keys (e.g. "1_stomper|easy").
    entt::hashed_string::hash_type current_key_hash{0};

    // Build "song_id|difficulty" into caller-supplied buf (no heap allocation).
    // Returns snprintf-style char count (excluding NUL).  cap should be KEY_CAP.
    static int32_t make_key_str(char* buf, int32_t cap,
                                const char* song_id, const char* difficulty) {
        return std::snprintf(buf, static_cast<std::size_t>(cap), "%s|%s", song_id, difficulty);
    }

    // Compute the FNV-1a 32-bit hash for a song+difficulty key (no heap allocation).
    static entt::hashed_string::hash_type make_key_hash(const char* song_id,
                                                        const char* difficulty) {
        char buf[KEY_CAP]{};
        make_key_str(buf, KEY_CAP, song_id, difficulty);
        // Cast to const char* to select the const_wrapper (runtime) overload of
        // hashed_string::value(), not the consteval array-literal overload.
        return entt::hashed_string::value(static_cast<const char*>(buf));
    }

    // Returns the stored score for key, or 0 if not found.
    int32_t get_score(const char* key) const {
        for (int32_t i = 0; i < entry_count; ++i) {
            if (std::strncmp(entries[i].key, key, KEY_CAP) == 0) {
                return entries[i].score;
            }
        }
        return 0;
    }

    // Returns the stored score for a hash — used on the session current-key path.
    // Hashes each stored key at lookup time; safe for N <= MAX_ENTRIES.
    int32_t get_score_by_hash(entt::hashed_string::hash_type hash) const {
        for (int32_t i = 0; i < entry_count; ++i) {
            if (entt::hashed_string::value(static_cast<const char*>(entries[i].key)) == hash) {
                return entries[i].score;
            }
        }
        return 0;
    }

    // Insert or update entry by string key.  Silently drops if table is full.
    void set_score(const char* key, int32_t score) {
        for (int32_t i = 0; i < entry_count; ++i) {
            if (std::strncmp(entries[i].key, key, KEY_CAP) == 0) {
                entries[i].score = score;
                return;
            }
        }
        if (entry_count < MAX_ENTRIES) {
            std::strncpy(entries[entry_count].key, key, KEY_CAP - 1);
            entries[entry_count].key[KEY_CAP - 1] = '\0';
            entries[entry_count].score = score;
            ++entry_count;
        }
    }

    // Update an existing entry's score by hash.  No-op if hash is not found
    // (entry must be pre-registered via ensure_entry before the session begins).
    void set_score_by_hash(entt::hashed_string::hash_type hash, int32_t score) {
        for (int32_t i = 0; i < entry_count; ++i) {
            if (entt::hashed_string::value(static_cast<const char*>(entries[i].key)) == hash) {
                entries[i].score = score;
                return;
            }
        }
    }

    // Ensure an entry exists for key with score 0 if not already present.
    // Call this alongside setting current_key_hash so that update_if_higher
    // can update the entry by hash without needing the key string again.
    void ensure_entry(const char* key) {
        for (int32_t i = 0; i < entry_count; ++i) {
            if (std::strncmp(entries[i].key, key, KEY_CAP) == 0) return;
        }
        if (entry_count < MAX_ENTRIES) {
            std::strncpy(entries[entry_count].key, key, KEY_CAP - 1);
            entries[entry_count].key[KEY_CAP - 1] = '\0';
            entries[entry_count].score = 0;
            ++entry_count;
        }
    }

    int32_t get_current_high_score() const {
        if (current_key_hash == 0) return 0;
        return get_score_by_hash(current_key_hash);
    }
};

struct HighScorePersistence {
    std::string path;
};
