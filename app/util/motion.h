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

// Approach-ring kinetic envelope. With reduce_motion the ring no longer
// continuously lerps in radius and fades over the entire approach window;
// instead it snaps on as a static "perfect-window-imminent" indicator
// once the obstacle is inside `near_threshold` of the perfect line.
// The function returns false when the ring should not be drawn at all.
//
// Inputs are the lerp `ratio` (0 = perfect/imminent, 1 = at the outer
// appearance distance) plus the static radius bounds.  Returns the
// (radius, alpha_scale) the caller should use; alpha_scale is in [0,1]
// and multiplies whatever base alpha the renderer would have used.
struct ApproachRing {
    float radius = 0.0f;
    float alpha_scale = 0.0f;  // 0 → don't draw
};

inline ApproachRing approach_ring_envelope(float ratio,
                                           float btn_radius,
                                           float max_ring_radius,
                                           bool reduce_motion,
                                           float near_threshold = 0.3f) {
    ApproachRing out{};
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    if (reduce_motion) {
        // Snap to a static indicator only once the timing window is
        // imminent; otherwise suppress the ring entirely so there is
        // no continuous size animation across the approach.
        if (ratio > near_threshold) return out;
        out.radius = max_ring_radius;
        out.alpha_scale = 1.0f;
        return out;
    }
    out.radius = btn_radius + (max_ring_radius - btn_radius) * ratio;
    out.alpha_scale = 1.0f - ratio * 0.5f;
    return out;
}

// Score-popup drift attenuation. Reduce-motion zeroes the upward drift
// velocity so the text stays anchored at its spawn position; the
// alpha-fade still expires the entity, the colour/text/value remain
// visible (informational channel intact).
inline float popup_drift_scale(bool reduce_motion) {
    return reduce_motion ? 0.0f : 1.0f;
}

// Particle decorative-velocity attenuation. Reduce-motion clamps the
// horizontal/vertical velocity of in-flight decorative particles so
// the burst is visibly calmer without removing the gameplay-state
// signal (the particles still spawn, age, and despawn on schedule).
inline float particle_velocity_scale(bool reduce_motion) {
    return reduce_motion ? 0.0f : 1.0f;
}

} // namespace motion
