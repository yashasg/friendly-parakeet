#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "entities/settings.h"
#include "tags/tags.h"
#include <filesystem>
#include <fstream>
#include "temp_paths.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("Settings: audio_offset_seconds() converts milliseconds correctly", "[settings]") {
    SettingsState state;
    state.audio_offset_ms = 100;
    CHECK_THAT(settings::audio_offset_seconds(state), WithinAbs(0.1f, 0.0001f));

    state.audio_offset_ms = -50;
    CHECK_THAT(settings::audio_offset_seconds(state), WithinAbs(-0.05f, 0.0001f));

    state.audio_offset_ms = 0;
    CHECK_THAT(settings::audio_offset_seconds(state), WithinAbs(0.0f, 0.0001f));
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
    CHECK_FALSE(settings::ftue_complete(state));
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
    CHECK(settings::ftue_complete(state));
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
    CHECK(settings::ftue_complete(state));

    nlohmann::json negative;
    negative["ftue_run_count"] = -3;

    result = settings::settings_from_json(negative, state);
    CHECK(result == true);
    CHECK(state.ftue_run_count == SettingsState::MIN_FTUE_RUN_COUNT);
    CHECK_FALSE(settings::ftue_complete(state));
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
    test_paths::ScopedPath scoped_dir{
        test_paths::unique_relative_path("test_settings_tmp")};
    const std::filesystem::path& test_dir = scoped_dir.path;
    std::filesystem::path test_file = test_dir / "settings.json";

    std::filesystem::create_directories(test_dir);

    // Create and save
    SettingsState original;
    original.audio_offset_ms = -75;
    original.haptics_enabled = false;
    original.reduce_motion = true;
    original.ftue_run_count = 1;

    const auto save_result = settings::save_settings(original, test_file);
    CHECK(save_result.status == persistence::Status::Success);
    CHECK(std::filesystem::exists(test_file));

    // Load and verify
    SettingsState loaded;
    const auto load_result = settings::load_settings(loaded, test_file);
    CHECK(load_result.status == persistence::Status::Success);
    CHECK(loaded.audio_offset_ms == -75);
    CHECK(loaded.haptics_enabled == false);
    CHECK(loaded.reduce_motion == true);
    CHECK(loaded.ftue_run_count == 1);

}

TEST_CASE("Settings persistence: save_settings supports current-directory files", "[settings]") {
    test_paths::ScopedPath scoped_file{
        test_paths::unique_relative_path("settings_current_dir_tmp.json")};
    const std::filesystem::path& test_file = scoped_file.path;
    std::filesystem::remove(test_file);

    SettingsState state;
    state.audio_offset_ms = 20;

    CHECK(settings::save_settings(state, test_file).ok());
    CHECK(std::filesystem::exists(test_file));

    SettingsState loaded;
    CHECK(settings::load_settings(loaded, test_file).ok());
    CHECK(loaded.audio_offset_ms == 20);

}

TEST_CASE("Settings persistence: load_settings returns false for missing file", "[settings]") {
    const std::filesystem::path nonexistent =
        test_paths::unique_relative_path("nonexistent_file_xyz.json");
    SettingsState state;

    const auto result = settings::load_settings(state, nonexistent);
    CHECK(result.status == persistence::Status::MissingFile);
}

TEST_CASE("Settings persistence: load_settings handles malformed JSON", "[settings]") {
    test_paths::ScopedPath scoped_dir{
        test_paths::unique_relative_path("test_settings_tmp")};
    const std::filesystem::path& test_dir = scoped_dir.path;
    std::filesystem::path test_file = test_dir / "bad_settings.json";

    std::filesystem::create_directories(test_dir);

    // Write malformed JSON
    std::ofstream out(test_file);
    out << "{ invalid json }";
    out.close();

    SettingsState state;
    const auto result = settings::load_settings(state, test_file);
    CHECK(result.status == persistence::Status::CorruptData);

}

TEST_CASE("Settings persistence: save_settings reports unwritable parent path", "[settings]") {
    test_paths::ScopedPath scoped_root{
        test_paths::unique_relative_path("test_settings_unwritable_parent")};
    const std::filesystem::path& root = scoped_root.path;
    std::filesystem::path not_a_dir = root / "not_a_directory";
    std::filesystem::path target = not_a_dir / "settings.json";

    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    {
        std::ofstream out(not_a_dir);
        REQUIRE(out.is_open());
        out << "file blocks directory creation";
    }

    SettingsState state;
    const auto result = settings::save_settings(state, target);
    CHECK(result.status == persistence::Status::DirectoryCreateFailed);

}

TEST_CASE("Settings persistence runtime: mark_dirty_and_save persists and clears dirty", "[settings][issue-303]") {
    test_paths::ScopedPath scoped_root{
        test_paths::unique_relative_path("test_settings_runtime_save")};
    const std::filesystem::path& root = scoped_root.path;
    const std::filesystem::path file = root / "settings.json";
    std::filesystem::remove_all(root);

    SettingsState state;
    state.audio_offset_ms = 30;
    state.haptics_enabled = false;
    state.reduce_motion = true;
    state.ftue_run_count = 1;

    entt::registry reg;
    SettingsPersistence persistence;
    persistence.path = file.string();
    const auto entity = create_settings_entity(reg, state, std::move(persistence));

    settings::mark_dirty_and_save(reg, state);
    CHECK_FALSE(reg.all_of<SettingsDirtyTag>(entity));
    CHECK(reg.get<SettingsPersistence>(entity).last_save.status == persistence::Status::Success);
    CHECK(std::filesystem::exists(file));

    SettingsState loaded;
    REQUIRE(settings::load_settings(loaded, file).ok());
    CHECK(loaded.audio_offset_ms == state.audio_offset_ms);
    CHECK(loaded.haptics_enabled == state.haptics_enabled);
    CHECK(loaded.reduce_motion == state.reduce_motion);
    CHECK(loaded.ftue_run_count == state.ftue_run_count);

}

TEST_CASE("Settings persistence runtime: mark_dirty_and_save keeps dirty when path unavailable",
          "[settings][issue-303]") {
    SettingsState state;
    entt::registry reg;
    const auto entity = create_settings_entity(reg, state);

    settings::mark_dirty_and_save(reg, state);

    CHECK(reg.all_of<SettingsDirtyTag>(entity));
    CHECK(reg.get<SettingsPersistence>(entity).last_save.status == persistence::Status::PathUnavailable);
}

TEST_CASE("Persistence paths: one shared policy resolves both settings and high-score files", "[settings]") {
    persistence::Paths paths;
    test_paths::ScopedPath scoped_root{
        test_paths::unique_relative_path("test_persistence_policy_root")};
    const auto result = persistence::resolve_paths(paths, scoped_root.path);
    REQUIRE(result.ok());
    CHECK(paths.settings_file == paths.root_dir / "settings.json");
    CHECK(paths.high_scores_file == paths.root_dir / "high_scores.json");
}

TEST_CASE("Persistence paths: nested root override creates parent directories recursively",
          "[settings][persistence][issue-1025]") {
    test_paths::ScopedPath scoped_root{
        test_paths::unique_relative_path("test_persistence_policy_nested_root")};
    const std::filesystem::path& root = scoped_root.path;
    const auto nested_root = root / "missing" / "parent" / "shapeshifter";
    std::filesystem::remove_all(root);

    persistence::Paths paths;
    const auto result = persistence::resolve_paths(paths, nested_root);
    REQUIRE(result.ok());
    CHECK(std::filesystem::is_directory(nested_root));
    CHECK(paths.settings_file == nested_root / "settings.json");
    CHECK(paths.high_scores_file == nested_root / "high_scores.json");

}

TEST_CASE("Persistence sync seams skip paths outside the web persistence root", "[settings]") {
    const std::filesystem::path current_dir_file =
        test_paths::unique_relative_path("settings_current_dir_tmp.json");
    CHECK(persistence::prepare_for_persistence_read(current_dir_file).ok());
    CHECK(persistence::flush_persistence_writes(current_dir_file).ok());
}

TEST_CASE("Web persistence initialization failures remain retryable",
          "[settings][persistence][issue-970]") {
    struct RetryContext {
        int calls{0};
        int failures_before_success{1};
    } context;

    auto bootstrap = [](void* raw_context) -> persistence::Result {
        auto& ctx = *static_cast<RetryContext*>(raw_context);
        ++ctx.calls;
        if (ctx.calls <= ctx.failures_before_success) {
            return {persistence::Status::FileReadFailed,
                    std::make_error_code(std::errc::io_error)};
        }
        return {};
    };

    persistence::WebPersistenceInitState state;
    CHECK_FALSE(persistence::ensure_web_persistence_ready(state, bootstrap, &context).ok());
    CHECK_FALSE(state.initialized);
    CHECK(context.calls == 1);

    CHECK(persistence::ensure_web_persistence_ready(state, bootstrap, &context).ok());
    CHECK(state.initialized);
    CHECK(context.calls == 2);

    CHECK(persistence::ensure_web_persistence_ready(state, bootstrap, &context).ok());
    CHECK(context.calls == 2);
}

TEST_CASE("Settings persistence helper: file path resolution reports failure without CWD fallback", "[settings]") {
    test_paths::ScopedPath scoped_root{
        test_paths::unique_relative_path("test_settings_path_resolution_failure")};
    const auto& root = scoped_root.path;
    const auto blocked_root = root / "blocked_root";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    {
        std::ofstream out(blocked_root);
        REQUIRE(out.is_open());
        out << "file blocks directory creation";
    }

    std::filesystem::path file_path =
        test_paths::unique_relative_path("seed_should_clear.json");
    const auto result = settings::get_settings_file_path(file_path, blocked_root);
    CHECK(result.status == persistence::Status::DirectoryCreateFailed);
    CHECK(file_path.empty());

}

TEST_CASE("Persistence paths: directory creation failure is observable", "[settings]") {
    test_paths::ScopedPath scoped_root{
        test_paths::unique_relative_path("test_persistence_policy_failure")};
    const auto& root = scoped_root.path;
    const auto blocked_root = root / "blocked_root";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    {
        std::ofstream out(blocked_root);
        REQUIRE(out.is_open());
        out << "file blocks directory creation";
    }

    persistence::Paths paths;
    const auto result = persistence::resolve_paths(paths, blocked_root);
    CHECK(result.status == persistence::Status::DirectoryCreateFailed);

}

TEST_CASE("Settings persistence: save_settings creates nested parent directories",
          "[settings][persistence][issue-1025]") {
    test_paths::ScopedPath scoped_root{
        test_paths::unique_relative_path("test_settings_nested_parent")};
    const auto& root = scoped_root.path;
    const auto file = root / "missing" / "parent" / "settings.json";
    std::filesystem::remove_all(root);

    SettingsState state;
    state.audio_offset_ms = -40;
    REQUIRE(settings::save_settings(state, file).ok());
    CHECK(std::filesystem::exists(file));

    SettingsState loaded;
    REQUIRE(settings::load_settings(loaded, file).ok());
    CHECK(loaded.audio_offset_ms == state.audio_offset_ms);

}
