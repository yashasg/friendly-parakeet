#include "settings_persistence.h"
#include "fs_utils.h"
#include "persistence_policy.h"
#include <fstream>
#include <algorithm>
#include <cstdint>
#include <limits>

namespace settings {

namespace {

bool json_integer_to_i64(const nlohmann::json& value, std::int64_t& out) {
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

}  // namespace

std::filesystem::path get_settings_dir() {
    persistence::Paths paths;
    const auto result = persistence::resolve_paths(paths);
    return result.ok() ? paths.root_dir : std::filesystem::path{};
}

persistence::Result get_settings_file_path(
    std::filesystem::path& out_path,
    const std::filesystem::path& root_override) {
    persistence::Paths paths;
    const auto result = persistence::resolve_paths(paths, root_override);
    if (!result.ok()) {
        out_path.clear();
        return result;
    }

    out_path = paths.settings_file;
    return persistence::Result{};
}

nlohmann::json settings_to_json(const SettingsState& state) {
    nlohmann::json obj;
    obj["audio_offset_ms"] = state.audio_offset_ms;
    obj["haptics_enabled"] = state.haptics_enabled;
    obj["reduce_motion"] = state.reduce_motion;
    obj["ftue_run_count"] = static_cast<int>(state.ftue_run_count);
    return obj;
}

bool settings_from_json(const nlohmann::json& obj, SettingsState& state) {
    if (!obj.is_object()) {
        return false;
    }

    SettingsState next;
    if (obj.contains("audio_offset_ms")) {
        const auto& value = obj["audio_offset_ms"];
        std::int64_t raw = 0;
        if (!json_integer_to_i64(value, raw)) return false;
        const auto clamped = std::clamp<std::int64_t>(
            raw,
            static_cast<std::int64_t>(SettingsState::MIN_AUDIO_OFFSET_MS),
            static_cast<std::int64_t>(SettingsState::MAX_AUDIO_OFFSET_MS));
        next.audio_offset_ms = static_cast<std::int16_t>(clamped);
    }
    if (obj.contains("haptics_enabled")) {
        const auto& value = obj["haptics_enabled"];
        if (!value.is_boolean()) return false;
        next.haptics_enabled = value.get<bool>();
    }
    if (obj.contains("reduce_motion")) {
        const auto& value = obj["reduce_motion"];
        if (!value.is_boolean()) return false;
        next.reduce_motion = value.get<bool>();
    }
    if (obj.contains("ftue_run_count")) {
        const auto& value = obj["ftue_run_count"];
        std::int64_t raw = 0;
        if (!json_integer_to_i64(value, raw)) return false;
        const auto clamped = std::clamp<std::int64_t>(
            raw,
            static_cast<std::int64_t>(SettingsState::MIN_FTUE_RUN_COUNT),
            static_cast<std::int64_t>(SettingsState::MAX_FTUE_RUN_COUNT));
        next.ftue_run_count = static_cast<std::uint8_t>(clamped);
    }

    state = next;
    return true;
}

persistence::Result load_settings(SettingsState& state, const std::filesystem::path& path) {
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
        if (!settings_from_json(obj, state)) {
            return persistence::Result{persistence::Status::CorruptData, {}};
        }
        return persistence::Result{};
    } catch (const nlohmann::json::exception&) {
        return persistence::Result{persistence::Status::CorruptData, {}};
    }
}

persistence::Result save_settings(const SettingsState& state, const std::filesystem::path& path) {
    const auto ensure = fs_utils::ensure_directory_result(path.parent_path());
    if (!ensure.ok) {
        return persistence::Result{persistence::Status::DirectoryCreateFailed, ensure.error};
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        return persistence::Result{persistence::Status::FileOpenFailed, {}};
    }

    nlohmann::json obj = settings_to_json(state);
    file << obj.dump(2);
    file.flush();
    if (!file.good()) {
        return persistence::Result{persistence::Status::FileWriteFailed, {}};
    }
    return persistence::Result{};
}

void clamp_audio_offset(SettingsState& state) {
    state.audio_offset_ms = std::clamp(
        state.audio_offset_ms,
        SettingsState::MIN_AUDIO_OFFSET_MS,
        SettingsState::MAX_AUDIO_OFFSET_MS);
}

void clamp_ftue_run_count(SettingsState& state) {
    state.ftue_run_count = static_cast<uint8_t>(std::clamp(
        static_cast<int>(state.ftue_run_count),
        static_cast<int>(SettingsState::MIN_FTUE_RUN_COUNT),
        static_cast<int>(SettingsState::MAX_FTUE_RUN_COUNT)));
}

void mark_ftue_complete(SettingsState& state) {
    if (state.ftue_run_count == 0) {
        state.ftue_run_count = 1;
    }
    clamp_ftue_run_count(state);
}

float audio_offset_seconds(const SettingsState& state) {
    return state.audio_offset_ms / 1000.0f;
}

bool ftue_complete(const SettingsState& state) {
    return state.ftue_run_count > 0;
}

} // namespace settings
