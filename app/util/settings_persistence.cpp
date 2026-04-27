#include "settings_persistence.h"
#include "fs_utils.h"
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <system_error>

#ifndef _WIN32
    #include <pwd.h>
    #include <unistd.h>
#endif

namespace settings {

namespace {

std::filesystem::path create_or_fallback(const std::filesystem::path& dir) {
    if (fs_utils::ensure_directory(dir)) return dir;
    return std::filesystem::path(".");
}

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
    #ifdef _WIN32
        const char* appdata = std::getenv("APPDATA");
        if (appdata) {
            return create_or_fallback(std::filesystem::path(appdata) / "shapeshifter");
        }
    #elif defined(__EMSCRIPTEN__)
        return create_or_fallback(std::filesystem::path("."));
    #else
        const char* home = std::getenv("HOME");
        if (!home) {
            const passwd* pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
        if (home) {
            return create_or_fallback(std::filesystem::path(home) / ".shapeshifter");
        }
    #endif
    // Fallback to current directory if no home found
    return std::filesystem::path(".");
}

std::filesystem::path get_settings_file_path() {
    return get_settings_dir() / "settings.json";
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

bool load_settings(SettingsState& state, const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
        return false;
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json obj;
        file >> obj;
        return settings_from_json(obj, state);
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

bool save_settings(const SettingsState& state, const std::filesystem::path& path) {
    if (!fs_utils::ensure_directory(path.parent_path())) {
        return false;
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }

    nlohmann::json obj = settings_to_json(state);
    file << obj.dump(2);
    file.flush();
    if (!file.good()) {
        return false;
    }
    return true;
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

} // namespace settings
