#include "high_score_persistence.h"
#include "json_file_io.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace high_score {

namespace {

inline entt::hashed_string::hash_type hash_key(const char* key) {
    return entt::hashed_string::value(key, std::strlen(key));
}

inline bool is_key_valid(const char* key) {
    return key && key[0] != '\0';
}

template <typename Predicate>
int32_t find_entry_index_if(const HighScoreState& state, Predicate&& predicate) {
    for (int32_t i = 0; i < state.entry_count; ++i) {
        if (predicate(state.entries[i])) {
            return i;
        }
    }
    return -1;
}

int32_t find_entry_index_by_key(const HighScoreState& state, const char* key) {
    return find_entry_index_if(state, [key](const HighScoreState::Entry& entry) {
        return std::strncmp(entry.key, key, HighScoreState::KEY_CAP) == 0;
    });
}

int32_t find_entry_index_by_hash(const HighScoreState& state, entt::hashed_string::hash_type hash) {
    return find_entry_index_if(state, [hash](const HighScoreState::Entry& entry) {
        return hash_key(entry.key) == hash;
    });
}

template <typename State>
auto* find_entry_by_hash(State& state, entt::hashed_string::hash_type hash) {
    const int32_t idx = find_entry_index_by_hash(state, hash);
    return idx >= 0 ? &state.entries[idx] : nullptr;
}

bool append_entry(HighScoreState& state, const char* key, int32_t score) {
    if (state.entry_count >= HighScoreState::MAX_ENTRIES) return false;
    auto& entry = state.entries[state.entry_count++];
    std::strncpy(entry.key, key, HighScoreState::KEY_CAP - 1);
    entry.key[HighScoreState::KEY_CAP - 1] = '\0';
    entry.score = score;
    return true;
}

}  // namespace

int32_t make_key_str(char* buf, int32_t cap, const char* song_id, const char* difficulty) {
    if (!buf || cap <= 0 || !song_id || !difficulty) {
        return 0;
    }
    return std::snprintf(buf, static_cast<std::size_t>(cap), "%s|%s", song_id, difficulty);
}

entt::hashed_string::hash_type make_key_hash(const char* song_id, const char* difficulty) {
    char buf[HighScoreState::KEY_CAP]{};
    if (make_key_str(buf, HighScoreState::KEY_CAP, song_id, difficulty) <= 0) {
        return 0;
    }
    return hash_key(buf);
}

int32_t get_score(const HighScoreState& state, const char* key) {
    if (!is_key_valid(key)) return 0;
    const int32_t idx = find_entry_index_by_key(state, key);
    return idx >= 0 ? state.entries[idx].score : 0;
}

void set_score(HighScoreState& state, const char* key, int32_t score) {
    if (!is_key_valid(key)) return;
    const int32_t idx = find_entry_index_by_key(state, key);
    if (idx >= 0) {
        state.entries[idx].score = score;
        return;
    }
    append_entry(state, key, score);
}

void ensure_entry(HighScoreState& state, const char* key) {
    if (!is_key_valid(key)) return;
    if (find_entry_index_by_key(state, key) >= 0) return;
    append_entry(state, key, 0);
}

int32_t get_current_high_score(const HighScoreState& state) {
    if (state.current_key_hash == 0) return 0;
    if (const auto* entry = find_entry_by_hash(state, state.current_key_hash)) {
        return entry->score;
    }
    return 0;
}

nlohmann::json high_score_state_to_json(const HighScoreState& state) {
    nlohmann::json result;
    nlohmann::json scores_obj;

    for (int32_t i = 0; i < state.entry_count; ++i) {
        scores_obj[state.entries[i].key] = state.entries[i].score;
    }

    result["scores"] = scores_obj;
    return result;
}

bool high_score_state_from_json(const nlohmann::json& obj, HighScoreState& state) {
    if (!obj.is_object()) {
        return false;
    }

    HighScoreState next;
    next.current_key_hash = state.current_key_hash; // preserve session key across load

    if (obj.contains("scores")) {
        const auto& scores_obj = obj["scores"];
        if (!scores_obj.is_object()) {
            return false;
        }

        for (auto it = scores_obj.begin(); it != scores_obj.end(); ++it) {
            const auto& key = it.key();
            const auto& value = it.value();

            std::int64_t raw = 0;
            if (!persistence::json_integer_to_i64(value, raw)) return false;

            // Clamp negative scores to 0, cap at max int32
            if (raw < 0) {
                raw = 0;
            } else if (raw > std::numeric_limits<std::int32_t>::max()) {
                raw = std::numeric_limits<std::int32_t>::max();
            }

            set_score(next, key.c_str(), static_cast<std::int32_t>(raw));
        }
    }

    state = next;
    return true;
}

persistence::Result load_high_scores(HighScoreState& state, const std::filesystem::path& path) {
    return persistence::load_state_file(state, path, high_score_state_from_json);
}

persistence::Result save_high_scores(const HighScoreState& state, const std::filesystem::path& path) {
    return persistence::save_state_file(state, path, high_score_state_to_json);
}

void mark_dirty_and_save(HighScorePersistence& persistence_state, const HighScoreState& state) {
    persistence::mark_dirty_and_save(
        persistence_state,
        [&state](const std::filesystem::path& path) { return save_high_scores(state, path); });
}

void update_if_higher(HighScoreState& state, int32_t new_score) {
    if (state.current_key_hash == 0) return;
    if (new_score < 0) new_score = 0;
    if (auto* entry = find_entry_by_hash(state, state.current_key_hash);
        entry && new_score > entry->score) {
        entry->score = new_score;
    }
}

}  // namespace high_score
