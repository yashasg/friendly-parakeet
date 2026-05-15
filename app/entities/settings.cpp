#include "settings.h"
#include <fstream>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

entt::entity create_settings_entity(entt::registry& reg,
                                    SettingsState state,
                                    SettingsPersistence persistence) {
    auto existing = reg.view<SettingsTag>();
    if (existing.begin() != existing.end()) {
        throw std::logic_error("SettingsTag entity already exists");
    }

    auto entity = reg.create();
    reg.emplace<SettingsTag>(entity);
    reg.emplace<SettingsState>(entity, state);
    reg.emplace<SettingsPersistence>(entity, std::move(persistence));
    return entity;
}

SettingsState* find_settings_state(entt::registry& reg) {
    auto view = reg.view<SettingsTag, SettingsState>();
    auto it = view.begin();
    if (it == view.end()) {
        return nullptr;
    }
    const auto entity = *it;
    if (++it != view.end()) {
        throw std::logic_error("multiple Settings entities exist");
    }
    return &view.get<SettingsState>(entity);
}

const SettingsState* find_settings_state(const entt::registry& reg) {
    auto view = reg.view<SettingsTag, const SettingsState>();
    auto it = view.begin();
    if (it == view.end()) {
        return nullptr;
    }
    const auto entity = *it;
    if (++it != view.end()) {
        throw std::logic_error("multiple Settings entities exist");
    }
    return &view.get<const SettingsState>(entity);
}

SettingsState& settings_state(entt::registry& reg) {
    if (auto* state = find_settings_state(reg)) {
        return *state;
    }
    throw std::logic_error("Settings entity is missing; call create_settings_entity() first");
}

const SettingsState& settings_state(const entt::registry& reg) {
    if (const auto* state = find_settings_state(reg)) {
        return *state;
    }
    throw std::logic_error("Settings entity is missing; call create_settings_entity() first");
}

SettingsPersistence* find_settings_persistence(entt::registry& reg) {
    auto view = reg.view<SettingsTag, SettingsPersistence>();
    auto it = view.begin();
    if (it == view.end()) {
        return nullptr;
    }
    const auto entity = *it;
    if (++it != view.end()) {
        throw std::logic_error("multiple Settings entities exist");
    }
    return &view.get<SettingsPersistence>(entity);
}

const SettingsPersistence* find_settings_persistence(const entt::registry& reg) {
    auto view = reg.view<SettingsTag, const SettingsPersistence>();
    auto it = view.begin();
    if (it == view.end()) {
        return nullptr;
    }
    const auto entity = *it;
    if (++it != view.end()) {
        throw std::logic_error("multiple Settings entities exist");
    }
    return &view.get<const SettingsPersistence>(entity);
}

SettingsPersistence& settings_persistence(entt::registry& reg) {
    if (auto* persistence = find_settings_persistence(reg)) {
        return *persistence;
    }
    throw std::logic_error("Settings entity is missing; call create_settings_entity() first");
}

const SettingsPersistence& settings_persistence(const entt::registry& reg) {
    if (const auto* persistence = find_settings_persistence(reg)) {
        return *persistence;
    }
    throw std::logic_error("Settings entity is missing; call create_settings_entity() first");
}

void destroy_settings_entity(entt::registry& reg) {
    auto view = reg.view<SettingsTag>();
    auto it = view.begin();
    if (it == view.end()) {
        return;
    }
    const auto entity = *it;
    if (++it != view.end()) {
        throw std::logic_error("multiple Settings entities exist");
    }
    reg.destroy(entity);
}

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
    const auto prepare_result = persistence::prepare_for_persistence_read(path);
    if (!prepare_result.ok()) {
        return prepare_result;
    }

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
    const auto ensure_error = persistence::ensure_directory_exists(path.parent_path());
    if (ensure_error) {
        return persistence::Result{persistence::Status::DirectoryCreateFailed, ensure_error};
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
    file.close();
    if (!file.good()) {
        return persistence::Result{persistence::Status::FileWriteFailed, {}};
    }
    return persistence::flush_persistence_writes(path);
}

void mark_dirty_and_save(SettingsPersistence& persistence_state, const SettingsState& state) {
    persistence_state.dirty = true;
    if (persistence_state.path.empty()) {
        persistence_state.last_save = persistence::Result{persistence::Status::PathUnavailable, {}};
        return;
    }

    persistence_state.last_save = save_settings(state, persistence_state.path);
    if (persistence_state.last_save.ok()) {
        persistence_state.dirty = false;
    }
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
