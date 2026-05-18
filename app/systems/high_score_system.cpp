#include "high_score_system.h"
#include "persistence_policy_system.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <system_error>
#include <vector>

#include <raylib.h>

namespace high_score {

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
    if (cap <= 0) {
        return -1;
    }
    return std::snprintf(buf, static_cast<std::size_t>(cap), "%s|%s", song_id, difficulty);
}

entt::hashed_string::hash_type make_key_hash(const char* song_id, const char* difficulty) {
    char buf[HighScoreState::KEY_CAP]{};
    make_key_str(buf, HighScoreState::KEY_CAP, song_id, difficulty);
    // Cast to const char* to select the const_wrapper (runtime) overload of
    // hashed_string::value(), not the consteval array-literal overload.
    return entt::hashed_string::value(static_cast<const char*>(buf));
}

namespace {

// Linear scan over the HighScoreEntry row table; returns entt::null if no
// matching row. The table caps at MAX_ENTRIES (9), so O(N) per lookup is
// the dense-flat-array equivalent of the prior inline `std::array` scan.
entt::entity find_entry_by_key(const entt::registry& reg, const char* key) {
    auto view = reg.view<HighScoreEntry>();
    for (auto entity : view) {
        const auto& entry = view.get<HighScoreEntry>(entity);
        if (std::strncmp(entry.key, key, HighScoreState::KEY_CAP) == 0) {
            return entity;
        }
    }
    return entt::null;
}

entt::entity find_entry_by_hash(const entt::registry& reg, entt::hashed_string::hash_type hash) {
    auto view = reg.view<HighScoreEntry>();
    for (auto entity : view) {
        if (view.get<HighScoreEntry>(entity).key_hash == hash) {
            return entity;
        }
    }
    return entt::null;
}

void write_key(HighScoreEntry& entry, const char* key) {
    std::strncpy(entry.key, key, HighScoreState::KEY_CAP - 1);
    entry.key[HighScoreState::KEY_CAP - 1] = '\0';
    entry.key_hash = entt::hashed_string::value(static_cast<const char*>(entry.key));
}

}  // namespace

int32_t entry_count(const entt::registry& reg) {
    return static_cast<int32_t>(reg.view<HighScoreEntry>().size());
}

int32_t get_score(const entt::registry& reg, const char* key) {
    const auto entity = find_entry_by_key(reg, key);
    if (entity == entt::null) return 0;
    return reg.get<HighScoreEntry>(entity).score;
}

int32_t get_score_by_hash(const entt::registry& reg, entt::hashed_string::hash_type hash) {
    const auto entity = find_entry_by_hash(reg, hash);
    if (entity == entt::null) return 0;
    return reg.get<HighScoreEntry>(entity).score;
}

bool set_score(entt::registry& reg, const char* key, int32_t score) {
    if (const auto existing = find_entry_by_key(reg, key); existing != entt::null) {
        reg.get<HighScoreEntry>(existing).score = score;
        return true;
    }
    if (entry_count(reg) < HighScoreState::MAX_ENTRIES) {
        const auto entity = reg.create();
        auto& entry = reg.emplace<HighScoreEntry>(entity);
        write_key(entry, key);
        entry.score = score;
        return true;
    }
    TraceLog(LOG_WARNING, "High score table full; dropping score for key '%s'", key);
    return false;
}

bool ensure_entry(entt::registry& reg, const char* key) {
    if (find_entry_by_key(reg, key) != entt::null) return true;
    if (entry_count(reg) < HighScoreState::MAX_ENTRIES) {
        const auto entity = reg.create();
        auto& entry = reg.emplace<HighScoreEntry>(entity);
        write_key(entry, key);
        entry.score = 0;
        return true;
    }
    TraceLog(LOG_WARNING, "High score table full; cannot create entry for key '%s'", key);
    return false;
}

int32_t get_current_high_score(const entt::registry& reg, const HighScoreSession& session) {
    if (session.key_hash == 0) return 0;
    return get_score_by_hash(reg, session.key_hash);
}

namespace {

nlohmann::json high_score_state_to_json(const entt::registry& reg) {
    nlohmann::json result;
    nlohmann::json scores_obj;

    auto view = reg.view<HighScoreEntry>();
    for (auto entity : view) {
        const auto& entry = view.get<HighScoreEntry>(entity);
        scores_obj[entry.key] = entry.score;
    }

    result["scores"] = scores_obj;
    return result;
}

bool high_score_state_from_json(const nlohmann::json& obj, entt::registry& reg) {
    if (!obj.is_object()) {
        return false;
    }

    // Stage into a local buffer first so a parse failure or overflow leaves
    // the registry's existing rows untouched (atomic-on-success).
    std::vector<HighScoreEntry> staged;
    staged.reserve(HighScoreState::MAX_ENTRIES);

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

            if (static_cast<int32_t>(staged.size()) >= HighScoreState::MAX_ENTRIES) {
                return false;
            }

            HighScoreEntry entry{};
            write_key(entry, key.c_str());
            entry.score = static_cast<std::int32_t>(raw);
            staged.push_back(entry);
        }
    }

    // Commit: drop existing rows then emplace the staged set.
    auto existing = reg.view<HighScoreEntry>();
    reg.destroy(existing.begin(), existing.end());
    for (const auto& entry : staged) {
        const auto ent = reg.create();
        reg.emplace<HighScoreEntry>(ent, entry);
    }
    return true;
}

}  // namespace

persistence::Result load_high_scores(entt::registry& reg, const std::filesystem::path& path) {
    const auto prepare_result = persistence::prepare_for_persistence_read(path);
    if (!prepare_result.ok()) {
        return prepare_result;
    }

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
        if (!high_score_state_from_json(obj, reg)) {
            return persistence::Result{persistence::Status::CorruptData, {}};
        }
        return persistence::Result{};
    } catch (const nlohmann::json::exception&) {
        return persistence::Result{persistence::Status::CorruptData, {}};
    }
}

persistence::Result save_high_scores(const entt::registry& reg, const std::filesystem::path& path) {
    const auto ensure_error = persistence::ensure_directory_exists(path.parent_path());
    if (ensure_error) {
        return persistence::Result{persistence::Status::DirectoryCreateFailed, ensure_error};
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        return persistence::Result{persistence::Status::FileOpenFailed, {}};
    }

    nlohmann::json obj = high_score_state_to_json(reg);
    file << obj.dump(2);
    file.flush();
    if (!file.good()) {
        return persistence::Result{persistence::Status::FileWriteFailed, {}};
    }
    file.close();
    if (!file.good()) {
        return persistence::Result{persistence::Status::FileWriteFailed, {}};
    }
    return persistence::flush_persistence_writes(path);
}

bool update_if_higher(entt::registry& reg, const HighScoreSession& session, int32_t new_score) {
    if (session.key_hash == 0) return false;
    if (new_score < 0) new_score = 0;
    int32_t stored = get_score_by_hash(reg, session.key_hash);
    if (new_score > stored) {
        const auto entity = find_entry_by_hash(reg, session.key_hash);
        if (entity == entt::null) {
            TraceLog(LOG_WARNING, "High score entry hash %u not found; score update skipped",
                     static_cast<unsigned int>(session.key_hash));
            return false;
        }
        reg.get<HighScoreEntry>(entity).score = new_score;
    }
    return true;
}

}  // namespace high_score
