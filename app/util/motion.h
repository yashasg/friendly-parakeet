#pragma once

// Motion calibration helpers (#478).
//
// Centralises the "reduce motion" attenuation logic so HUD/popup/particle
// callers do not each re-implement the conditional. When SettingsState
// `reduce_motion` is true these helpers return their static/baseline value
// (no oscillation) while preserving the *informational* state — fill
// levels, hit-tier colour, and other non-decorative cues remain visible
// per the TestFlight A11Y baseline §2.3.2 (A11Y-08, AX-07).
//
// Pure, header-only, no dependencies on raylib so they are trivially
// unit-testable from the Catch2 suite.

#include <cmath>

namespace motion {

// Energy-bar idle bounce. Returns 0 when reduce_motion is on or the song
// is not advancing (no beat clock yet). Otherwise returns a per-beat
// (1 - phase)^3 envelope in [0,1].
inline float energy_bar_bounce(float song_time, float beat_period, bool reduce_motion) {
    if (reduce_motion) return 0.0f;
    if (beat_period <= 0.0f) return 0.0f;
    float phase = std::fmod(song_time, beat_period) / beat_period;
    if (phase < 0.0f) phase = 0.0f;
    float b = 1.0f - phase;
    return b * b * b;
}

// Critical-energy pulse. With reduce_motion the return is pinned to the
// static midpoint (0.5) so the *colour* still carries the critical-state
// information without the flashing oscillation.
inline float energy_critical_pulse(float pulse_time, bool reduce_motion) {
    if (reduce_motion) return 0.5f;
    return 0.5f + 0.5f * std::sin(pulse_time * 10.0f);
}

// Damage/feedback flash overlay strength. Reduce motion drops the overlay
// to zero; the underlying bar colour change still conveys the hit.
inline float flash_overlay_strength(float flash_ratio, bool reduce_motion) {
    if (reduce_motion) return 0.0f;
    return flash_ratio;
}

} // namespace motion
