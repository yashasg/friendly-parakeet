#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "components/settings.h"
#include "util/settings_persistence.h"
#include <filesystem>
#include <fstream>

using Catch::Matchers::WithinAbs;

TEST_CASE("Settings: audio_offset_seconds() converts milliseconds correctly", "[settings]") {
    SettingsState state;
    state.audio_offset_ms = 100;
    CHECK_THAT(state.audio_offset_seconds(), WithinAbs(0.1f, 0.0001f));

    state.audio_offset_ms = -50;
    CHECK_THAT(state.audio_offset_seconds(), WithinAbs(-0.05f, 0.0001f));

    state.audio_offset_ms = 0;
    CHECK_THAT(state.audio_offset_seconds(), WithinAbs(0.0f, 0.0001f));
}

TEST_CASE("Settings: clamp_audio_offset() restricts to [-250, 250]", "[settings]") {
    SettingsState state;

    state.audio_offset_ms = 500;
    settings::clamp_audio_offset(state);
    CHECK(state.audio_offset_ms == 250);

    state.audio_offset_ms = -500;
    settings::clamp_audio_offset(state);
    CHECK(state.audio_offset_ms == -250);

    state.audio_offset_ms = 0;
    settings::clamp_audio_offset(state);
    CHECK(state.audio_offset_ms == 0);
}

TEST_CASE("Settings: default values are correct", "[settings]") {
    SettingsState state;
    CHECK(state.audio_offset_ms == 0);
    CHECK(state.haptics_enabled == true);
    CHECK(state.reduce_motion == false);
    CHECK(state.ftue_run_count == 0);
    CHECK_FALSE(state.ftue_complete());
}

TEST_CASE("Settings persistence: settings_to_json produces correct structure", "[settings]") {
    SettingsState state;
    state.audio_offset_ms = 50;
    state.haptics_enabled = false;
    state.reduce_motion = true;
    state.ftue_run_count = 1;

    auto json = settings::settings_to_json(state);
    CHECK(json["audio_offset_ms"] == 50);
    CHECK(json["haptics_enabled"] == false);
    CHECK(json["reduce_motion"] == true);
    CHECK(json["ftue_run_count"] == 1);
}

TEST_CASE("Settings persistence: settings_from_json loads values correctly", "[settings]") {
    nlohmann::json obj;
    obj["audio_offset_ms"] = 100;
    obj["haptics_enabled"] = false;
    obj["reduce_motion"] = true;
    obj["ftue_run_count"] = 1;

    SettingsState state;
    bool result = settings::settings_from_json(obj, state);
    CHECK(result == true);
    CHECK(state.audio_offset_ms == 100);
    CHECK(state.haptics_enabled == false);
    CHECK(state.reduce_motion == true);
    CHECK(state.ftue_run_count == 1);
    CHECK(state.ftue_complete());
}

TEST_CASE("Settings persistence: settings_from_json clamps audio_offset", "[settings]") {
    nlohmann::json obj;
    obj["audio_offset_ms"] = 500;  // Out of range
    obj["haptics_enabled"] = true;
    obj["reduce_motion"] = false;

    SettingsState state;
    bool result = settings::settings_from_json(obj, state);
    CHECK(result == true);
    CHECK(state.audio_offset_ms == 250);  // Clamped
}

TEST_CASE("Settings persistence: settings_from_json clamps ftue_run_count", "[settings][ftue]") {
    nlohmann::json high;
    high["ftue_run_count"] = 99;

    SettingsState state;
    bool result = settings::settings_from_json(high, state);
    CHECK(result == true);
    CHECK(state.ftue_run_count == SettingsState::MAX_FTUE_RUN_COUNT);
    CHECK(state.ftue_complete());

    nlohmann::json negative;
    negative["ftue_run_count"] = -3;

    result = settings::settings_from_json(negative, state);
    CHECK(result == true);
    CHECK(state.ftue_run_count == SettingsState::MIN_FTUE_RUN_COUNT);
    CHECK_FALSE(state.ftue_complete());
}

TEST_CASE("Settings persistence: settings_from_json handles missing fields", "[settings]") {
    nlohmann::json obj;
    obj["haptics_enabled"] = false;
    // Missing audio_offset_ms and reduce_motion

    SettingsState state;
    bool result = settings::settings_from_json(obj, state);
    CHECK(result == true);
    CHECK(state.audio_offset_ms == 0);  // Default
    CHECK(state.haptics_enabled == false);
    CHECK(state.reduce_motion == false);  // Default
    CHECK(state.ftue_run_count == 0);  // Default for existing settings files
}

TEST_CASE("Settings persistence: settings_from_json rejects malformed field types", "[settings]") {
    SettingsState state;
    state.audio_offset_ms = 40;
    state.haptics_enabled = false;
    state.reduce_motion = true;

    CHECK_FALSE(settings::settings_from_json(nlohmann::json::array(), state));
    CHECK(state.audio_offset_ms == 40);
    CHECK_FALSE(state.haptics_enabled);
    CHECK(state.reduce_motion);

    nlohmann::json bad_offset = {
        {"audio_offset_ms", "late"},
        {"haptics_enabled", true},
        {"reduce_motion", false}
    };
    CHECK_FALSE(settings::settings_from_json(bad_offset, state));
    CHECK(state.audio_offset_ms == 40);

    nlohmann::json bad_toggle = {
        {"audio_offset_ms", 0},
        {"haptics_enabled", 1},
        {"reduce_motion", false}
    };
    CHECK_FALSE(settings::settings_from_json(bad_toggle, state));
    CHECK_FALSE(state.haptics_enabled);

    nlohmann::json bad_ftue = {
        {"audio_offset_ms", 0},
        {"haptics_enabled", true},
        {"reduce_motion", false},
        {"ftue_run_count", "done"}
    };
    CHECK_FALSE(settings::settings_from_json(bad_ftue, state));
    CHECK(state.ftue_run_count == 0);
}

TEST_CASE("Settings persistence: round-trip save and load", "[settings]") {
    std::filesystem::path test_dir = "test_settings_tmp";
    std::filesystem::path test_file = test_dir / "settings.json";

    std::filesystem::create_directories(test_dir);

    // Create and save
    SettingsState original;
    original.audio_offset_ms = -75;
    original.haptics_enabled = false;
    original.reduce_motion = true;
    original.ftue_run_count = 1;

    bool save_result = settings::save_settings(original, test_file);
    CHECK(save_result == true);
    CHECK(std::filesystem::exists(test_file));

    // Load and verify
    SettingsState loaded;
    bool load_result = settings::load_settings(loaded, test_file);
    CHECK(load_result == true);
    CHECK(loaded.audio_offset_ms == -75);
    CHECK(loaded.haptics_enabled == false);
    CHECK(loaded.reduce_motion == true);
    CHECK(loaded.ftue_run_count == 1);

    // Cleanup
    std::filesystem::remove_all(test_dir);
}

TEST_CASE("Settings persistence: save_settings supports current-directory files", "[settings]") {
    std::filesystem::path test_file = "settings_current_dir_tmp.json";
    std::filesystem::remove(test_file);

    SettingsState state;
    state.audio_offset_ms = 20;

    CHECK(settings::save_settings(state, test_file));
    CHECK(std::filesystem::exists(test_file));

    SettingsState loaded;
    CHECK(settings::load_settings(loaded, test_file));
    CHECK(loaded.audio_offset_ms == 20);

    std::filesystem::remove(test_file);
}

TEST_CASE("Settings persistence: load_settings returns false for missing file", "[settings]") {
    std::filesystem::path nonexistent = "nonexistent_file_xyz.json";
    SettingsState state;

    bool result = settings::load_settings(state, nonexistent);
    CHECK(result == false);
}

TEST_CASE("Settings persistence: load_settings handles malformed JSON", "[settings]") {
    std::filesystem::path test_dir = "test_settings_tmp";
    std::filesystem::path test_file = test_dir / "bad_settings.json";

    std::filesystem::create_directories(test_dir);

    // Write malformed JSON
    std::ofstream out(test_file);
    out << "{ invalid json }";
    out.close();

    SettingsState state;
    bool result = settings::load_settings(state, test_file);
    CHECK(result == false);

    // Cleanup
    std::filesystem::remove_all(test_dir);
}
