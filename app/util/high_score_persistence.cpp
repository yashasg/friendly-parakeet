#include "high_score_persistence.h"
#include "fs_utils.h"
#include "persistence_policy.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>

namespace high_score {

std::filesystem::path get_high_scores_dir() {
    persistence::Paths paths;
    const auto result = persistence::resolve_paths(paths);
    return result.ok() ? paths.root_dir : std::filesystem::path{};
}

persistence::Result get_high_scores_file_path(
    std::filesystem::path& out_path,
    const std::filesystem::path& root_override) {
    persistence::Paths paths;
    const auto result = persistence::resolve_paths(paths, root_override);
    if (!result.ok()) {
        out_path.clear();
        return result;
    }

    out_path = paths.high_scores_file;
    return persistence::Result{};
}

int32_t make_key_str(char* buf, int32_t cap, const char* song_id, const char* difficulty) {
    return std::snprintf(buf, static_cast<std::size_t>(cap), "%s|%s", song_id, difficulty);
}

entt::hashed_string::hash_type make_key_hash(const char* song_id, const char* difficulty) {
    char buf[HighScoreState::KEY_CAP]{};
    make_key_str(buf, HighScoreState::KEY_CAP, song_id, difficulty);
    // Cast to const char* to select the const_wrapper (runtime) overload of
    // hashed_string::value(), not the consteval array-literal overload.
    return entt::hashed_string::value(static_cast<const char*>(buf));
}

int32_t get_score(const HighScoreState& state, const char* key) {
    for (int32_t i = 0; i < state.entry_count; ++i) {
        if (std::strncmp(state.entries[i].key, key, HighScoreState::KEY_CAP) == 0) {
            return state.entries[i].score;
        }
    }
    return 0;
}

int32_t get_score_by_hash(const HighScoreState& state, entt::hashed_string::hash_type hash) {
    for (int32_t i = 0; i < state.entry_count; ++i) {
        if (entt::hashed_string::value(static_cast<const char*>(state.entries[i].key)) == hash) {
            return state.entries[i].score;
        }
    }
    return 0;
}

void set_score(HighScoreState& state, const char* key, int32_t score) {
    for (int32_t i = 0; i < state.entry_count; ++i) {
        if (std::strncmp(state.entries[i].key, key, HighScoreState::KEY_CAP) == 0) {
            state.entries[i].score = score;
            return;
        }
    }
    if (state.entry_count < HighScoreState::MAX_ENTRIES) {
        std::strncpy(state.entries[state.entry_count].key, key, HighScoreState::KEY_CAP - 1);
        state.entries[state.entry_count].key[HighScoreState::KEY_CAP - 1] = '\0';
        state.entries[state.entry_count].score = score;
        ++state.entry_count;
    }
}

void set_score_by_hash(HighScoreState& state, entt::hashed_string::hash_type hash, int32_t score) {
    for (int32_t i = 0; i < state.entry_count; ++i) {
        if (entt::hashed_string::value(static_cast<const char*>(state.entries[i].key)) == hash) {
            state.entries[i].score = score;
            return;
        }
    }
}

void ensure_entry(HighScoreState& state, const char* key) {
    for (int32_t i = 0; i < state.entry_count; ++i) {
        if (std::strncmp(state.entries[i].key, key, HighScoreState::KEY_CAP) == 0) return;
    }
    if (state.entry_count < HighScoreState::MAX_ENTRIES) {
        std::strncpy(state.entries[state.entry_count].key, key, HighScoreState::KEY_CAP - 1);
        state.entries[state.entry_count].key[HighScoreState::KEY_CAP - 1] = '\0';
        state.entries[state.entry_count].score = 0;
        ++state.entry_count;
    }
}

int32_t get_current_high_score(const HighScoreState& state) {
    if (state.current_key_hash == 0) return 0;
    return get_score_by_hash(state, state.current_key_hash);
}

namespace {

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

            if (!value.is_number_integer()) {
                return false;
            }

            std::int64_t raw = 0;
            if (value.is_number_unsigned()) {
                const auto unsigned_raw = value.get<std::uint64_t>();
                raw = unsigned_raw > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
                    ? std::numeric_limits<std::int64_t>::max()
                    : static_cast<std::int64_t>(unsigned_raw);
            } else {
                raw = value.get<std::int64_t>();
            }

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

}  // namespace

persistence::Result load_high_scores(HighScoreState& state, const std::filesystem::path& path) {
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        return persistence::Result{persistence::Status::FileReadFailed, ec};
    }
    if (!exists) {
        return persistence::Result{persistence::Status::MissingFile, {}};
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return persistence::Result{persistence::Status::FileOpenFailed, {}};
        }

        nlohmann::json obj;
        file >> obj;
        if (file.bad()) {
            return persistence::Result{persistence::Status::FileReadFailed, {}};
        }
        if (!high_score_state_from_json(obj, state)) {
            return persistence::Result{persistence::Status::CorruptData, {}};
        }
        return persistence::Result{};
    } catch (const nlohmann::json::exception&) {
        return persistence::Result{persistence::Status::CorruptData, {}};
    }
}

persistence::Result save_high_scores(const HighScoreState& state, const std::filesystem::path& path) {
    const auto ensure = fs_utils::ensure_directory_result(path.parent_path());
    if (!ensure.ok) {
        return persistence::Result{persistence::Status::DirectoryCreateFailed, ensure.error};
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        return persistence::Result{persistence::Status::FileOpenFailed, {}};
    }

    nlohmann::json obj = high_score_state_to_json(state);
    file << obj.dump(2);
    file.flush();
    if (!file.good()) {
        return persistence::Result{persistence::Status::FileWriteFailed, {}};
    }
    file.close();
    return persistence::Result{};
}

void update_if_higher(HighScoreState& state, int32_t new_score) {
    if (state.current_key_hash == 0) return;
    if (new_score < 0) new_score = 0;
    int32_t stored = get_score_by_hash(state, state.current_key_hash);
    if (new_score > stored) {
        set_score_by_hash(state, state.current_key_hash, new_score);
    }
}

}  // namespace high_score
