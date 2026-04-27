#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

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
    int32_t     entry_count{0};
    std::string current_key; // set once per session; not in hot-path

    static std::string make_key(const std::string& song_id, const std::string& difficulty) {
        return song_id + "|" + difficulty;
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
    int32_t get_score(const std::string& key) const { return get_score(key.c_str()); }

    // Insert or update entry.  Silently drops if table is full.
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
    void set_score(const std::string& key, int32_t score) { set_score(key.c_str(), score); }

    int32_t get_current_high_score() const {
        if (current_key.empty()) return 0;
        return get_score(current_key.c_str());
    }

    void update_current_high_score(int32_t score) {
        if (current_key.empty()) return;
        if (score < 0) score = 0;
        int32_t stored = get_score(current_key.c_str());
        if (score > stored) {
            set_score(current_key.c_str(), score);
        }
    }

    void set_current_key(const std::string& song_id, const std::string& difficulty) {
        current_key = make_key(song_id, difficulty);
    }
};

struct HighScorePersistence {
    std::string path;
};
