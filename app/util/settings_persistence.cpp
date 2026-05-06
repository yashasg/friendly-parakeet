#include "settings_persistence.h"
#include "json_file_io.h"
#include <algorithm>
#include <cstdint>

namespace settings {

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
    if (!persistence::read_optional_clamped_integer(obj, "audio_offset_ms",
                                                    SettingsState::MIN_AUDIO_OFFSET_MS,
                                                    SettingsState::MAX_AUDIO_OFFSET_MS,
                                                    next.audio_offset_ms)) return false;
    if (!persistence::read_optional_bool(obj, "haptics_enabled", next.haptics_enabled)) return false;
    if (!persistence::read_optional_bool(obj, "reduce_motion", next.reduce_motion)) return false;
    if (!persistence::read_optional_clamped_integer(obj, "ftue_run_count",
                                                    SettingsState::MIN_FTUE_RUN_COUNT,
                                                    SettingsState::MAX_FTUE_RUN_COUNT,
                                                    next.ftue_run_count)) return false;

    state = next;
    return true;
}

persistence::Result load_settings(SettingsState& state, const std::filesystem::path& path) {
    return persistence::load_state_file(state, path, settings_from_json);
}

persistence::Result save_settings(const SettingsState& state, const std::filesystem::path& path) {
    return persistence::save_state_file(state, path, settings_to_json);
}

void mark_dirty_and_save(SettingsPersistence& persistence_state, const SettingsState& state) {
    persistence::mark_dirty_and_save(
        persistence_state,
        [&state](const std::filesystem::path& path) { return save_settings(state, path); });
}

void clamp_audio_offset(SettingsState& state) {
    state.audio_offset_ms = std::clamp(
        state.audio_offset_ms,
        SettingsState::MIN_AUDIO_OFFSET_MS,
        SettingsState::MAX_AUDIO_OFFSET_MS);
}

float audio_offset_seconds(const SettingsState& state) {
    return state.audio_offset_ms / 1000.0f;
}

bool ftue_complete(const SettingsState& state) {
    return state.ftue_run_count > 0;
}

} // namespace settings
