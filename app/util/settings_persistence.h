#pragma once

#include "../components/settings.h"
#include <nlohmann/json.hpp>
#include <filesystem>

namespace settings {

// Load settings from JSON file (platform-aware path).
// If file doesn't exist or parsing fails, returns false.
// Settings are clamped to valid ranges.
bool load_settings(SettingsState& state, const std::filesystem::path& path);

// Save settings to JSON file (platform-aware path).
// Returns false if write fails.
bool save_settings(const SettingsState& state, const std::filesystem::path& path);

// Get platform-specific settings directory.
// On desktop: ~/.shapeshifter or %APPDATA%\shapeshifter
// On web/mobile where a home directory is unavailable, falls back to ".".
std::filesystem::path get_settings_dir();

// Get full settings file path (dir/settings.json)
std::filesystem::path get_settings_file_path();

// Convert SettingsState to JSON
nlohmann::json settings_to_json(const SettingsState& state);

// Load settings from JSON object, with clamping and validation.
// Returns true if successful, false if JSON is malformed.
bool settings_from_json(const nlohmann::json& obj, SettingsState& state);

} // namespace settings
