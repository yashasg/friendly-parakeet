#pragma once

#include "persistence_policy.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>

#include <nlohmann/json.hpp>

namespace persistence {

inline bool json_integer_to_i64(const nlohmann::json& value, std::int64_t& out) {
    if (!value.is_number_integer()) return false;
    if (value.is_number_unsigned()) {
        const auto unsigned_raw = value.get<std::uint64_t>();
        out = unsigned_raw > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
            ? std::numeric_limits<std::int64_t>::max()
            : static_cast<std::int64_t>(unsigned_raw);
    } else {
        out = value.get<std::int64_t>();
    }
    return true;
}

template <typename IntType>
inline bool read_optional_clamped_integer(const nlohmann::json& obj,
                                          const char* key,
                                          IntType min_value,
                                          IntType max_value,
                                          IntType& out_value) {
    if (!obj.contains(key)) return true;
    std::int64_t raw = 0;
    if (!json_integer_to_i64(obj.at(key), raw)) return false;
    const auto clamped = std::clamp<std::int64_t>(
        raw,
        static_cast<std::int64_t>(min_value),
        static_cast<std::int64_t>(max_value));
    out_value = static_cast<IntType>(clamped);
    return true;
}

inline bool read_optional_bool(const nlohmann::json& obj, const char* key, bool& out_value) {
    if (!obj.contains(key)) return true;
    const auto& value = obj.at(key);
    if (!value.is_boolean()) return false;
    out_value = value.get<bool>();
    return true;
}

inline Result load_json_file(nlohmann::json& out, const std::filesystem::path& path) {
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        return Result{Status::FileReadFailed, ec};
    }
    if (!exists) {
        return Result{Status::MissingFile, {}};
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return Result{Status::FileOpenFailed, {}};
    }

    try {
        file >> out;
    } catch (const nlohmann::json::exception&) {
        return Result{Status::CorruptData, {}};
    }
    if (file.bad()) {
        return Result{Status::FileReadFailed, {}};
    }

    return Result{};
}

inline Result save_json_file(const nlohmann::json& in, const std::filesystem::path& path) {
    const std::filesystem::path parent_dir = path.parent_path();
    if (!parent_dir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent_dir, ec);
        if (ec) {
            return Result{Status::DirectoryCreateFailed, ec};
        }
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        return Result{Status::FileOpenFailed, {}};
    }

    file << in.dump(2);
    file.flush();
    if (!file.good()) {
        return Result{Status::FileWriteFailed, {}};
    }

    return Result{};
}

template <typename State, typename DeserializeFn>
inline Result load_state_file(State& out_state,
                              const std::filesystem::path& path,
                              DeserializeFn&& deserialize) {
    nlohmann::json obj;
    const Result load_result = load_json_file(obj, path);
    if (!load_result.ok()) {
        return load_result;
    }
    if (!deserialize(obj, out_state)) {
        return Result{Status::CorruptData, {}};
    }
    return Result{};
}

template <typename State, typename SerializeFn>
inline Result save_state_file(const State& state,
                              const std::filesystem::path& path,
                              SerializeFn&& serialize) {
    return save_json_file(serialize(state), path);
}

template <typename PersistenceState, typename SaveFn>
inline void mark_dirty_and_save(PersistenceState& persistence_state, SaveFn&& save_fn) {
    persistence_state.dirty = true;
    if (persistence_state.path.empty()) {
        persistence_state.last_save = Result{Status::PathUnavailable, {}};
        return;
    }

    persistence_state.last_save = save_fn(persistence_state.path);
    if (persistence_state.last_save.ok()) {
        persistence_state.dirty = false;
    }
}

}  // namespace persistence
