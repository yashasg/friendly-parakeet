#include "gameplay_hud_ring.h"

#include "../constants.h"
#include "../util/rhythm_math.h"

#include <raymath.h>  // Clamp

namespace {

float gameplay_hud_timing_distance(const SongState* song_state, float timing_seconds) {
    const float scroll_speed = (song_state && song_state->scroll_speed > 0.0f)
        ? song_state->scroll_speed
        : constants::BASE_SCROLL_SPEED;
    return scroll_speed * timing_seconds;
}

}  // namespace

float gameplay_hud_perfect_distance(const SongState* song_state) {
    return gameplay_hud_timing_distance(song_state, kTimingPerfectSeconds);
}

float gameplay_hud_good_distance(const SongState* song_state) {
    return gameplay_hud_timing_distance(song_state, kTimingGoodSeconds);
}

float gameplay_hud_ok_distance(const SongState* song_state) {
    return gameplay_hud_timing_distance(song_state, kTimingOkSeconds);
}

float gameplay_hud_ring_ratio(float nearest_dist, float perfect_dist, float ring_appear_dist) {
    const float denom = ring_appear_dist - perfect_dist;
    if (denom <= 0.0f) return 0.0f;
    return Clamp((nearest_dist - perfect_dist) / denom, 0.0f, 1.0f);
}

GameplayHudRingCue gameplay_hud_ring_cue(float nearest_dist,
                                         float perfect_dist,
                                         float good_dist,
                                         float ring_appear_dist) {
    if (nearest_dist <= 0.0f || nearest_dist >= ring_appear_dist) return {};
    if (nearest_dist <= perfect_dist) return {true, kGameplayHudRingPerfectColor};
    if (nearest_dist <= good_dist)    return {true, kGameplayHudRingNearColor};
    return {true, kGameplayHudRingFarColor};
}

ApproachRingEnvelope approach_ring_envelope(float ratio,
                                            float btn_radius,
                                            float max_ring_radius,
                                            bool reduce_motion,
                                            float near_threshold) {
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
