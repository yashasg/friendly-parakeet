#pragma once

#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <filesystem>
#include <string>

#include "../util/persistence_policy.h"

struct SettingsTag {};

struct SettingsState {
    static constexpr int16_t MIN_AUDIO_OFFSET_MS = -250;
    static constexpr int16_t MAX_AUDIO_OFFSET_MS = 250;
    static constexpr int16_t AUDIO_OFFSET_STEP_MS = 10;
    static constexpr uint8_t MIN_FTUE_RUN_COUNT = 0;
    static constexpr uint8_t MAX_FTUE_RUN_COUNT = 5;

    // Audio offset in milliseconds, bounded [-250, 250]
    // Positive offset delays beat timing; negative offset advances it.
    int16_t audio_offset_ms = 0;

    // Haptics enabled (default true, opt-out)
    bool haptics_enabled = true;

    // Reduce motion for accessibility
    bool reduce_motion = false;

    // Minimal TestFlight FTUE progress.
    // 0 = tutorial incomplete; 1+ = tutorial completed/unlocked.
    uint8_t ftue_run_count = 0;
};

struct SettingsPersistence {
    std::string path;
    persistence::Result last_load{};
    persistence::Result last_save{};
    bool dirty{false};
};

// Creates the singleton settings entity and returns its handle.
// Throws std::logic_error if the registry already has a SettingsTag entity.
entt::entity create_settings_entity(entt::registry& reg,
                                    SettingsState state = {},
                                    SettingsPersistence persistence = {});

SettingsState* find_settings_state(entt::registry& reg);
const SettingsState* find_settings_state(const entt::registry& reg);
SettingsState& settings_state(entt::registry& reg);
const SettingsState& settings_state(const entt::registry& reg);

SettingsPersistence* find_settings_persistence(entt::registry& reg);
const SettingsPersistence* find_settings_persistence(const entt::registry& reg);
SettingsPersistence& settings_persistence(entt::registry& reg);
const SettingsPersistence& settings_persistence(const entt::registry& reg);

void destroy_settings_entity(entt::registry& reg);

namespace settings {

persistence::Result load_settings(SettingsState& state, const std::filesystem::path& path);
persistence::Result save_settings(const SettingsState& state, const std::filesystem::path& path);
void mark_dirty_and_save(SettingsPersistence& persistence_state, const SettingsState& state);
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
