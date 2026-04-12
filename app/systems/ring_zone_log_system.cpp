#include "all_systems.h"
#include "../components/ring_zone.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/transform.h"
#include "../components/rhythm.h"
#include "../components/game_state.h"
#include "../session_logger.h"
#include "../enum_names.h"
#include "../constants.h"

#include <cmath>

// Zone encoding: 1=Bad, 2=Ok, 3=Good, 4=Perfect
// Returns 0 if outside all zones (ring visible but no timing zone yet).
static int8_t compute_ring_zone(float dist, bool past_center,
                                float bad_far, float ok_far,
                                float good_far, float perf_far,
                                float good_near, float ok_near,
                                float bad_near) {
    if (!past_center) {
        // Approaching (far side)
        if (dist > bad_far)  return 0;  // no zone yet
        if (dist > ok_far)   return 1;  // Bad (early)
        if (dist > good_far) return 2;  // Ok (early)
        if (dist > perf_far) return 3;  // Good (early)
        return 4;                        // Perfect
    } else {
        // Departing (near side)
        if (dist > good_near) return 4;  // Still Perfect
        if (dist > ok_near)   return 3;  // Good (late)
        if (dist > bad_near)  return 2;  // Ok (late)
        return 1;                         // Bad (late)
    }
}

static const char* zone_label(int8_t zone, bool past_center) {
    switch (zone) {
        case 1: return past_center ? "Bad(late)"     : "Bad(early)";
        case 2: return past_center ? "Ok(late)"      : "Ok(early)";
        case 3: return past_center ? "Good(late)"    : "Good(early)";
        case 4: return "Perfect";
        default: return "None";
    }
}

void ring_zone_log_system(entt::registry& reg, float /*dt*/) {
    auto* log = reg.ctx().find<SessionLog>();
    if (!log || !log->file) return;

    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto* song = reg.ctx().find<SongState>();
    if (!song) return;

    float v = song->scroll_speed;
    float M = song->morph_duration;
    float H = song->half_window;

    // Precompute zone boundary distances
    float bad_far    = v * (M + 1.75f * H);
    float ok_far     = v * (M + 1.50f * H);
    float good_far   = v * (M + 1.25f * H);
    float perf_far   = v * (M + 1.00f * H);  // = perfect center distance
    float perf_center= perf_far;
    float good_near  = v * (M + 0.50f * H);
    float ok_near    = v * (M + 0.25f * H);
    float bad_near   = v * M;

    auto view = reg.view<Position, RingZoneTracker, RequiredShape>(
        entt::exclude<ScoredTag>);

    for (auto [entity, pos, tracker, req] : view.each()) {
        float dist = constants::PLAYER_Y - pos.y;
        if (dist <= 0.0f) continue;

        // Update past_center flag
        if (dist <= perf_center && !tracker.past_center) {
            tracker.past_center = true;
        }

        int8_t zone = compute_ring_zone(dist, tracker.past_center,
                                         bad_far, ok_far, good_far, perf_far,
                                         good_near, ok_near, bad_near);

        // Log on zone transition (skip zone 0 → 0)
        if (zone != tracker.last_zone && zone > 0) {
            session_log_write(*log, song->song_time, "GAME",
                "RING_ZONE obstacle=%u zone=%s shape=%s dist=%.0fpx",
                static_cast<unsigned>(entt::to_integral(entity)),
                zone_label(zone, tracker.past_center),
                shape_name(req.shape), dist);
        }
        tracker.last_zone = zone;
    }
}
