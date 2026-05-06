#pragma once

#include "settings.h"
#include <nlohmann/json.hpp>
#include <filesystem>

namespace settings {

// Load settings from JSON file (platform-aware path).
// Distinguishes missing/corrupt/path errors via structured status.
// Settings are clamped to valid ranges.
persistence::Result load_settings(SettingsState& state, const std::filesystem::path& path);

// Save settings to JSON file (platform-aware path).
// Distinguishes path/open/write failures via structured status.
persistence::Result save_settings(const SettingsState& state, const std::filesystem::path& path);

// Mark settings persistence as dirty and attempt to save immediately.
// Keeps dirty=true when save cannot complete so callers can retry later.
void mark_dirty_and_save(SettingsPersistence& persistence_state, const SettingsState& state);

// Convert SettingsState to JSON
nlohmann::json settings_to_json(const SettingsState& state);

// Load settings from JSON object, with clamping and validation.
// Returns true if successful, false if JSON is malformed.
bool settings_from_json(const nlohmann::json& obj, SettingsState& state);

// Clamp audio_offset_ms to valid range [MIN_AUDIO_OFFSET_MS, MAX_AUDIO_OFFSET_MS].
void clamp_audio_offset(SettingsState& state);

// Convert audio_offset_ms to seconds.
float audio_offset_seconds(const SettingsState& state);

// Returns true if FTUE has been completed (ftue_run_count > 0).
bool ftue_complete(const SettingsState& state);

} // namespace settings
