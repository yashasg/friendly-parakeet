#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../util/rhythm_math.h"
#include "../components/song_state.h"
#include "../constants.h"
#include <raylib.h>
#include <cmath>

namespace {

Rectangle centered_rect(float cx, float cy, float w, float h) {
    return {cx - w * 0.5f, cy - h * 0.5f, w, h};
}

Vector2 player_timing_point(const WorldTransform& transform, const VerticalState& vstate) {
    return {0.0f, transform.position.y + vstate.y_offset};
}

bool player_in_timing_window(const WorldTransform& player_transform,
                              const VerticalState& vstate,
                              float obstacle_z) {
    Rectangle timing_window = {
        0.0f,
        obstacle_z - constants::COLLISION_MARGIN,
        1.0f,
        constants::COLLISION_MARGIN * 2.0f
    };
    return CheckCollisionPointRec(player_timing_point(player_transform, vstate), timing_window);
}

bool player_overlaps_lane(const WorldTransform& player_transform, const Position& obstacle_pos) {
    Rectangle player_lane = centered_rect(player_transform.position.x, 0.0f,
                                          constants::PLAYER_SIZE, 1.0f);
    Rectangle obstacle_lane = centered_rect(obstacle_pos.x, 0.0f, constants::PLAYER_SIZE, 1.0f);
    return CheckCollisionRecs(player_lane, obstacle_lane);
}

}  // namespace

void collision_system(entt::registry& reg, float /*dt*/) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto player_view = reg.view<PlayerTag, WorldTransform, PlayerShape, ShapeWindow, Lane, VerticalState>();
    if (player_view.begin() == player_view.end()) return;

    auto player_it = player_view.begin();
    auto [p_transform, p_shape, p_window, p_lane, p_vstate] =
        player_view.get<WorldTransform, PlayerShape, ShapeWindow, Lane, VerticalState>(*player_it);

    auto* song    = reg.ctx().find<SongState>();
    auto* results = reg.ctx().find<SongResults>();
    bool rhythm_mode = (song != nullptr);

    // resolve: tag entity as scored (cleared) or missed.
    // kind is passed by the caller — no try_get needed since each per-kind
    // loop already holds the Obstacle component.
    auto resolve = [&](entt::entity entity, float obs_z, bool cleared) {
        if (!player_in_timing_window(p_transform, p_vstate, obs_z)) return;

        if (cleared) {
            // In rhythm mode, compute timing grade
            if (rhythm_mode) {
                auto window_phase = p_window.phase;
                if (window_phase == WindowPhase::Active && song->half_window > 0.0f) {
                    // Grade timing by comparing the player's predicted peak
                    // (center of the Active window) against the obstacle's
                    // scheduled arrival.  peak_time is set at press time as
                    // press + morph_duration + half_window, so a perfectly-
                    // timed tap gives peak_time ≈ arrival_time → pct ≈ 0.
                    // A stale press from a previous beat has peak_time far
                    // from the current obstacle's arrival → Bad.
                    auto* beat_info = reg.try_get<BeatInfo>(entity);
                    float reference_time = beat_info ? beat_info->arrival_time
                                                     : p_window.peak_time;
                    float pct_from_peak = std::abs(p_window.peak_time - reference_time) / song->half_window;
                    if (pct_from_peak > 1.0f) pct_from_peak = 1.0f;
                    TimingTier tier = compute_timing_tier(pct_from_peak);
                    reg.emplace<TimingGrade>(entity, tier, 1.0f - pct_from_peak);

                    if (results) {
                        switch (tier) {
                            case TimingTier::Perfect: results->perfect_count++; break;
                            case TimingTier::Good:    results->good_count++;    break;
                            case TimingTier::Ok:      results->ok_count++;      break;
                            case TimingTier::Bad:     results->bad_count++;     break;
                        }
                    }

                    if (!p_window.graded) {
                        float scale = window_scale_for_tier(tier);
                        float remaining = song->window_duration - p_window.window_timer;
                        if (remaining > 0.0f) {
                            if (scale < 1.0f) {
                                // Adjust window_start backward so the song-time-derived
                                // elapsed time is naturally larger on subsequent ticks,
                                // causing the Active phase to expire earlier.  Mutating
                                // window_timer directly would be overwritten by
                                // shape_window_system on the next frame.
                                p_window.window_start -= remaining * (1.0f - scale);
                            }
                        }
                        p_window.window_scale = scale;
                        p_window.graded = true;
                    }
                }
            }
            reg.emplace<ScoredTag>(entity);
        } else {
            // MISS — tag only; scoring_system owns energy drain and death-cause attribution.
            reg.emplace<MissTag>(entity);
            reg.emplace<ScoredTag>(entity);
        }
    };

    // Per-kind structural views — each loop touches only entities that actually
    // carry the required components, eliminating per-entity try_get branches.

    // ShapeGate: RequiredShape only (no BlockedLanes, no RequiredLane)
    {
        auto view = reg.view<ObstacleTag, Position, RequiredShape>(
            entt::exclude<ScoredTag, BlockedLanes, RequiredLane>);
        for (auto [e, pos, req] : view.each()) {
            bool shape_match = (p_shape.current == req.shape) && (p_shape.current != Shape::Hexagon);
            bool lane_match  = player_overlaps_lane(p_transform, pos);
            resolve(e, pos.y, shape_match && lane_match);
        }
    }

    // LaneBlock: BlockedLanes only (no RequiredShape)
    {
        auto view = reg.view<ObstacleTag, Position, BlockedLanes>(
            entt::exclude<ScoredTag, RequiredShape>);
        for (auto [e, pos, blocked] : view.each()) {
            resolve(e, pos.y, !((blocked.mask >> p_lane.current) & 1));
        }
    }

    // LowBar / HighBar: RequiredVAction
    {
        auto view = reg.view<ObstacleTag, ObstacleScrollZ, RequiredVAction>(
            entt::exclude<ScoredTag>);
        for (auto [e, oz, req_v] : view.each()) {
            resolve(e, oz.z, p_vstate.mode == req_v.action);
        }
    }

    // ComboGate: RequiredShape + BlockedLanes (no RequiredLane)
    {
        auto view = reg.view<ObstacleTag, Position, RequiredShape, BlockedLanes>(
            entt::exclude<ScoredTag, RequiredLane>);
        for (auto [e, pos, req, blocked] : view.each()) {
            bool shape_ok = (p_shape.current == req.shape) && (p_shape.current != Shape::Hexagon);
            bool lane_ok  = !((blocked.mask >> p_lane.current) & 1);
            resolve(e, pos.y, shape_ok && lane_ok);
        }
    }

    // SplitPath: RequiredShape + RequiredLane
    {
        auto view = reg.view<ObstacleTag, Position, RequiredShape, RequiredLane>(
            entt::exclude<ScoredTag>);
        for (auto [e, pos, req, rlane] : view.each()) {
            bool shape_ok = (p_shape.current == req.shape) && (p_shape.current != Shape::Hexagon);
            bool lane_ok  = (p_lane.current == rlane.lane);
            resolve(e, pos.y, shape_ok && lane_ok);
        }
    }

    // LanePushLeft / LanePushRight: no data components (structural negative).
    // Passive: auto-pushes player when in the same lane; always scores.
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle>(
            entt::exclude<ScoredTag, RequiredShape, BlockedLanes, RequiredLane, RequiredVAction>);
        for (auto [e, pos, obs] : view.each()) {
            if (!player_in_timing_window(p_transform, p_vstate, pos.y)) continue;
            bool on_same_lane = player_overlaps_lane(p_transform, pos);
            if (on_same_lane && p_lane.target < 0) {
                int8_t delta = (obs.kind == ObstacleKind::LanePushLeft) ? -1 : 1;
                int8_t dest  = static_cast<int8_t>(p_lane.current + delta);
                if (dest >= 0 && dest < constants::LANE_COUNT) {
                    p_lane.target = dest;
                    p_lane.lerp_t = 0.0f;
                }
            }
            reg.emplace<ScoredTag>(e);
        }
    }
}
