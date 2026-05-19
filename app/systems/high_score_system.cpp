#include "high_score_system.h"
#include "persistence_policy_system.h"
#include <nlohmann/json.hpp>
#include <algorithm>
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
    const bool valid_target = buf != nullptr && cap > 0;
    const int32_t written = valid_target
        ? std::snprintf(buf, static_cast<std::size_t>(cap), "%s|%s", song_id, difficulty)
        : -1;
    const bool fits = written >= 0 && written < cap;
    if (valid_target && !fits) {
        buf[0] = '\0';
    }
    return fits ? written : -1;
}

entt::hashed_string::hash_type make_key_hash(const char* song_id, const char* difficulty) {
    char buf[HighScoreState::KEY_CAP]{};
    const int32_t key_len = make_key_str(buf, HighScoreState::KEY_CAP, song_id, difficulty);
    // Cast to const char* to select the const_wrapper (runtime) overload of
    // hashed_string::value(), not the consteval array-literal overload.
    return key_len >= 0 ? entt::hashed_string::value(static_cast<const char*>(buf)) : 0;
}

namespace {

// Linear scan over the HighScoreEntry row table; returns entt::null if no
// matching row. The table caps at MAX_ENTRIES (9), so O(N) per lookup is
// the dense-flat-array equivalent of the prior inline `std::array` scan.
entt::entity find_entry_by_key(const entt::registry& reg, const char* key) {
    const bool valid_key = key != nullptr && std::strlen(key) < HighScoreState::KEY_CAP;
    entt::entity match = entt::null;
    auto view = reg.view<HighScoreEntry>();
    for (auto entity : view) {
        const auto& entry = view.get<HighScoreEntry>(entity);
        match = (valid_key && match == entt::null && std::strncmp(entry.key, key, HighScoreState::KEY_CAP) == 0)
            ? entity
            : match;
    }
    return match;
}

entt::entity find_entry_by_hash(const entt::registry& reg, entt::hashed_string::hash_type hash) {
    entt::entity match = entt::null;
    auto view = reg.view<HighScoreEntry>();
    for (auto entity : view) {
        match = (match == entt::null && view.get<HighScoreEntry>(entity).key_hash == hash)
            ? entity
            : match;
    }
    return match;
}

bool write_key(HighScoreEntry& entry, const char* key) {
    if (key == nullptr || std::strlen(key) >= HighScoreState::KEY_CAP) {
        return false;
    }
    std::strncpy(entry.key, key, HighScoreState::KEY_CAP);
    entry.key_hash = entt::hashed_string::value(static_cast<const char*>(entry.key));
    return true;
}

}  // namespace

int32_t entry_count(const entt::registry& reg) {
    return static_cast<int32_t>(reg.view<HighScoreEntry>().size());
}

int32_t get_score(const entt::registry& reg, const char* key) {
    const auto entity = find_entry_by_key(reg, key);
    return entity == entt::null ? 0 : reg.get<HighScoreEntry>(entity).score;
}

int32_t get_score_by_hash(const entt::registry& reg, entt::hashed_string::hash_type hash) {
    const auto entity = find_entry_by_hash(reg, hash);
    return entity == entt::null ? 0 : reg.get<HighScoreEntry>(entity).score;
}

bool set_score(entt::registry& reg, const char* key, int32_t score) {
    if (key == nullptr || std::strlen(key) >= HighScoreState::KEY_CAP) {
        TraceLog(LOG_WARNING, "High score key too long; dropping score for key '%s'",
                 key == nullptr ? "<null>" : key);
        return false;
    }
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
    if (key == nullptr || std::strlen(key) >= HighScoreState::KEY_CAP) {
        TraceLog(LOG_WARNING, "High score key too long; cannot create entry for key '%s'",
                 key == nullptr ? "<null>" : key);
        return false;
    }
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

            raw = std::clamp(
                raw,
                std::int64_t{0},
                static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()));

            if (static_cast<int32_t>(staged.size()) >= HighScoreState::MAX_ENTRIES) {
                return false;
            }

            HighScoreEntry entry{};
            if (!write_key(entry, key.c_str())) {
                return false;
            }
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
    new_score = std::max(new_score, 0);
    const auto entity = find_entry_by_hash(reg, session.key_hash);
    if (entity == entt::null) {
        TraceLog(LOG_WARNING, "High score entry hash %u not found; score update skipped",
                 static_cast<unsigned int>(session.key_hash));
        return false;
    }
    auto& entry = reg.get<HighScoreEntry>(entity);
    const int32_t stored = entry.score;
    if (new_score > stored) {
        entry.score = new_score;
    }
    return true;
}

}  // namespace high_score
