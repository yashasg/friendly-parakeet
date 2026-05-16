#pragma once

#include <entt/entt.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>

#include "../components/settings.h"

// Singleton accessors over the SettingsTag entity. find_* return nullptr when
// the entity is absent; the reference-returning forms throw std::logic_error.
SettingsState* find_settings_state(entt::registry& reg);
const SettingsState* find_settings_state(const entt::registry& reg);
SettingsState& settings_state(entt::registry& reg);
const SettingsState& settings_state(const entt::registry& reg);

SettingsPersistence* find_settings_persistence(entt::registry& reg);
const SettingsPersistence* find_settings_persistence(const entt::registry& reg);
SettingsPersistence& settings_persistence(entt::registry& reg);
const SettingsPersistence& settings_persistence(const entt::registry& reg);

namespace settings {

persistence::Result load_settings(SettingsState& state, const std::filesystem::path& path);
persistence::Result save_settings(const SettingsState& state, const std::filesystem::path& path);
// Mark the settings as needing a save and attempt persistence. On success
// the SettingsDirtyTag is removed from the settings entity; on failure the
// tag remains so a later call retries. The settings entity must exist
// (created via create_settings_entity).
void mark_dirty_and_save(entt::registry& reg, const SettingsState& state);
persistence::Result get_settings_file_path(
    std::filesystem::path& out_path,
    const std::filesystem::path& root_override = {});
nlohmann::json settings_to_json(const SettingsState& state);
bool settings_from_json(const nlohmann::json& obj, SettingsState& state);
void clamp_audio_offset(SettingsState& state);
void clamp_ftue_run_count(SettingsState& state);
void mark_ftue_complete(SettingsState& state);
float audio_offset_seconds(const SettingsState& state);
bool ftue_complete(const SettingsState& state);

} // namespace settings
