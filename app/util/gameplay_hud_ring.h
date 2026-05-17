#pragma once

#include <raylib.h>
#include <raymath.h>  // Clamp

#include "../components/song_state.h"
#include "../constants.h"
#include "rhythm_math.h"

// Gameplay HUD approach-ring math (issue #1297).
//
// All helpers below are pure transforms — no registry, no global state —
// so the envelope system, the renderer, and the unit tests share the same
// arithmetic. Per Fabian's existential processing (issue #1202/#1204) the
// former `GameplayHudRingCue` returns the data the renderer needs (visibility
// + color) instead of a label discriminator that the renderer would have
// to switch on.

struct GameplayHudRingCue {
    bool  visible = false;
    Color color   = {0, 0, 0, 0};

    friend constexpr bool operator==(const GameplayHudRingCue& a,
                                     const GameplayHudRingCue& b) {
        return a.visible == b.visible
            && a.color.r == b.color.r && a.color.g == b.color.g
            && a.color.b == b.color.b && a.color.a == b.color.a;
    }
};

inline constexpr Color kGameplayHudRingPerfectColor{100, 255, 100, 255};
inline constexpr Color kGameplayHudRingNearColor   {180, 255, 100, 255};
inline constexpr Color kGameplayHudRingFarColor    {120, 120, 180, 255};

namespace detail {

inline float gameplay_hud_timing_distance(const SongState* song_state, float timing_seconds) {
    const float scroll_speed = (song_state && song_state->scroll_speed > 0.0f)
        ? song_state->scroll_speed
        : constants::BASE_SCROLL_SPEED;
    return scroll_speed * timing_seconds;
}

}  // namespace detail

// Per-tier scroll distances from the player Y to the obstacle at the
// matching timing window edge. Scale by `SongState::scroll_speed` so a
// faster song produces a wider ring envelope.
inline float gameplay_hud_perfect_distance(const SongState* song_state) {
    return detail::gameplay_hud_timing_distance(song_state, kTimingPerfectSeconds);
}

inline float gameplay_hud_good_distance(const SongState* song_state) {
    return detail::gameplay_hud_timing_distance(song_state, kTimingGoodSeconds);
}

inline float gameplay_hud_ok_distance(const SongState* song_state) {
    return detail::gameplay_hud_timing_distance(song_state, kTimingOkSeconds);
}

// Linear ramp from `ring_appear_dist` (ratio = 1) to `perfect_dist`
// (ratio = 0). Used by the envelope to lerp the ring radius from the
// button radius up to the max ring radius.
inline float gameplay_hud_ring_ratio(float nearest_dist, float perfect_dist,
                                     float ring_appear_dist) {
    const float denom = ring_appear_dist - perfect_dist;
    if (denom <= 0.0f) return 0.0f;
    return Clamp((nearest_dist - perfect_dist) / denom, 0.0f, 1.0f);
}

// Picks the ring color (Far / Near / Perfect) based on which timing
// window the nearest obstacle currently occupies. `visible == false`
// means the obstacle is outside the appear window and the ring should
// not be drawn.
inline GameplayHudRingCue gameplay_hud_ring_cue(float nearest_dist,
                                                float perfect_dist,
                                                float good_dist,
                                                float ring_appear_dist) {
    if (nearest_dist <= 0.0f || nearest_dist >= ring_appear_dist) return {};
    if (nearest_dist <= perfect_dist) return {true, kGameplayHudRingPerfectColor};
    if (nearest_dist <= good_dist)    return {true, kGameplayHudRingNearColor};
    return {true, kGameplayHudRingFarColor};
}

// Combined envelope output written into the `ApproachRing` component on
// each lane button entity. `radius` is the actual ring radius (lerped
// from `btn_radius` at appear toward `max_ring_radius` at perfect);
// `alpha_scale` is the multiplicative alpha (peaks at 1.0, fades 0.5 by
// perfect). The reduce-motion branch suppresses the lerp/fade and snaps
// to a static "imminent" indicator (issue #534).
struct ApproachRingEnvelope {
    float radius      = 0.0f;
    float alpha_scale = 0.0f;
};

inline ApproachRingEnvelope approach_ring_envelope(float ratio,
                                                   float btn_radius,
                                                   float max_ring_radius,
                                                   bool reduce_motion,
                                                   float near_threshold = 0.3f) {
    ApproachRingEnvelope out{};
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    if (reduce_motion) {
        if (ratio > near_threshold) return out;
        out.radius = max_ring_radius;
        out.alpha_scale = 1.0f;
        return out;
    }
    out.radius = btn_radius + (max_ring_radius - btn_radius) * ratio;
    out.alpha_scale = 1.0f - ratio * 0.5f;
    return out;
}
