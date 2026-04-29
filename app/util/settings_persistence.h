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

// Get platform-specific settings directory.
// Returns empty path when resolution fails.
std::filesystem::path get_settings_dir();

// Resolve full settings file path.
// Returns structured failure and clears out_path when resolution fails.
persistence::Result get_settings_file_path(
    std::filesystem::path& out_path,
    const std::filesystem::path& root_override = {});

// Convert SettingsState to JSON
nlohmann::json settings_to_json(const SettingsState& state);

// Load settings from JSON object, with clamping and validation.
// Returns true if successful, false if JSON is malformed.
bool settings_from_json(const nlohmann::json& obj, SettingsState& state);

// Clamp audio_offset_ms to valid range [MIN_AUDIO_OFFSET_MS, MAX_AUDIO_OFFSET_MS].
void clamp_audio_offset(SettingsState& state);

// Clamp ftue_run_count to valid range [MIN_FTUE_RUN_COUNT, MAX_FTUE_RUN_COUNT].
void clamp_ftue_run_count(SettingsState& state);

// Mark FTUE as completed: sets ftue_run_count to 1 if currently 0, then clamps.
void mark_ftue_complete(SettingsState& state);

// Convert audio_offset_ms to seconds.
float audio_offset_seconds(const SettingsState& state);

// Returns true if FTUE has been completed (ftue_run_count > 0).
bool ftue_complete(const SettingsState& state);

} // namespace settings
