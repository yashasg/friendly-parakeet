#pragma once

#include "../components/rhythm.h"
#include "../constants.h"

// Better timing -> smaller scale -> remaining Active window collapses sooner.
// collision_system applies this via window_start adjustment (scale < 1.0 path).
// Spec: rhythm-spec.md §5/§7. BAD is treated as a miss; window is left unchanged.
inline float window_scale_for_tier(TimingTier tier) {
    switch (tier) {
        case TimingTier::Perfect: return 0.50f;
        case TimingTier::Good:    return 0.75f;
        case TimingTier::Ok:      return 1.00f;
        case TimingTier::Bad:     return 1.00f;
    }
    return 1.00f;
}

inline float timing_multiplier(TimingTier tier) {
    switch (tier) {
        case TimingTier::Perfect: return 1.50f;
        case TimingTier::Good:    return 1.00f;
        case TimingTier::Ok:      return 0.50f;
        case TimingTier::Bad:     return 0.25f;
    }
    return 0.25f;
}

inline TimingTier compute_timing_tier(float pct_from_peak) {
    if (pct_from_peak <= 0.25f) return TimingTier::Perfect;
    if (pct_from_peak <= 0.50f) return TimingTier::Good;
    if (pct_from_peak <= 0.75f) return TimingTier::Ok;
    return TimingTier::Bad;
}

inline void song_state_compute_derived(SongState& s) {
    s.beat_period     = 60.0f / s.bpm;
    s.lead_time       = s.lead_beats * s.beat_period;

    s.scroll_speed    = constants::APPROACH_DIST / s.lead_time;

    constexpr float BASE_WINDOW_BEATS = 1.6f;
    constexpr float MIN_WINDOW        = 0.36f;
    constexpr float BASE_MORPH_BEATS  = 0.2f;
    constexpr float MIN_MORPH         = 0.06f;

    float bpm_scale = (s.bpm > 130.0f) ? (130.0f / s.bpm) : 1.0f;
    s.window_duration = BASE_WINDOW_BEATS * s.beat_period * bpm_scale;
    if (s.window_duration < MIN_WINDOW) s.window_duration = MIN_WINDOW;
    s.half_window     = s.window_duration / 2.0f;

    s.morph_duration  = BASE_MORPH_BEATS * s.beat_period;
    if (s.morph_duration < MIN_MORPH) s.morph_duration = MIN_MORPH;
}
