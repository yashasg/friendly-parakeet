#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

// Settings state singleton (lives in registry context)
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

    // Helper: convert audio offset to seconds
    float audio_offset_seconds() const {
        return audio_offset_ms / 1000.0f;
    }

    // Clamp audio offset to valid range [-250, 250]
    void clamp_audio_offset() {
        audio_offset_ms = std::clamp(audio_offset_ms, MIN_AUDIO_OFFSET_MS, MAX_AUDIO_OFFSET_MS);
    }

    void clamp_ftue_run_count() {
        ftue_run_count = std::clamp(ftue_run_count, MIN_FTUE_RUN_COUNT, MAX_FTUE_RUN_COUNT);
    }

    bool ftue_complete() const {
        return ftue_run_count > 0;
    }

    void mark_ftue_complete() {
        if (ftue_run_count == 0) {
            ftue_run_count = 1;
        }
        clamp_ftue_run_count();
    }
};

struct SettingsPersistence {
    std::string path;
};
