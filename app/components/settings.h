#pragma once

#include <cstdint>
#include <string>

#include "../systems/persistence_policy_system.h"

// Settings entity component data — read by gameplay/UI systems each frame.
//
// Field bounds enforced by settings::clamp_audio_offset / clamp_ftue_run_count
// (see systems/settings_system.h). Values outside the constants below get
// pulled back into range on load.
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

// Persistence I/O bookkeeping for the settings file (path + last load/save
// results + dirty bit). Lives on the singleton settings entity alongside
// SettingsState; mutated by systems/settings_system.
struct SettingsPersistence {
    std::string path;
    persistence::Result last_load{};
    persistence::Result last_save{};
    bool dirty{false};
};
