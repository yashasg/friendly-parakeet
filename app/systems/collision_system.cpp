#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../util/rhythm_math.h"
#include "../components/song_state.h"
#include "../components/gameplay_intents.h"
#include "../constants.h"
#include <cmath>

namespace {

Rectangle centered_hitbox_rect(float x) {
    constexpr float kHitboxEdgePadding = 1.0e-4f;
    const float size = constants::PLAYER_SIZE + kHitboxEdgePadding;
    return {x - size * 0.5f, 0.0f, size, 1.0f};
}

bool player_matches_required_shape(const PlayerShape& p_shape,
                                    const ShapeWindow& p_window,
                                    Shape required) {
    if (p_shape.current == required && p_shape.current != Shape::Hexagon) {
        return true;
    }
    // Press-time judgment: during an active/morphing window, the selected target
    // shape should satisfy shape-gates even before visual morph completion.
    if (p_window.phase != WindowPhase::Idle &&
        p_window.target_shape == required &&
        p_window.target_shape != Shape::Hexagon) {
        return true;
    }
    return false;
}

}  // namespace

void collision_system(entt::registry& reg, float /*dt*/) {
    auto player_view = reg.view<PlayerTag, WorldTransform, PlayerShape, ShapeWindow, Lane, VerticalState>();
    if (player_view.begin() == player_view.end()) return;

    auto player_entity = *player_view.begin();
    auto [p_transform, p_shape, p_window, p_lane, p_vstate] =
        player_view.get<WorldTransform, PlayerShape, ShapeWindow, Lane, VerticalState>(player_entity);

    auto* song_ptr = reg.ctx().find<SongState>();
    if (!song_ptr) {
        song_ptr = &reg.ctx().emplace<SongState>();
    }
    auto& song = *song_ptr;

    // Frame-constant precomputes — both values are invariant across all obstacle
    // loops since player transform and vertical state don't change mid-frame.
    // Precomputing here avoids redundant addition + Vector2 construction per obstacle.
    const float player_timing_y = p_transform.position.y + p_vstate.y_offset;
    const float player_x        = p_transform.position.x;

    // resolve: tag entity as scored (cleared) or missed.
    // kind is passed by the caller — no try_get needed since each per-kind
    // loop already holds the Obstacle component.
    auto resolve = [&](entt::entity entity, float obs_z, bool cleared) {
        if (obs_z < player_timing_y) return;

        if (!cleared) {
            // MISS — tag only; scoring_system owns energy drain and death-cause attribution.
            reg.emplace<MissTag>(entity);
            reg.emplace<ScoredTag>(entity);
            return;
        }

        reg.emplace<ScoredTag>(entity);
    };

    const bool can_grade_shape =
        song.half_window > 0.0f && p_window.press_time >= 0.0f;
    auto grade_shape_timing = [&](entt::entity entity, float arrival_time) {
        float delta_seconds = std::abs(p_window.press_time - arrival_time);
        TimingTier tier = compute_timing_tier_from_delta(delta_seconds);
        float precision = 1.0f - (delta_seconds / kTimingOkSeconds);
        if (precision < 0.0f) precision = 0.0f;
        if (precision > 1.0f) precision = 1.0f;
        reg.emplace<TimingGrade>(entity, tier, precision);

        if (!p_window.graded) {
            float scale = window_scale_for_tier(tier);
            float remaining = song.window_duration - p_window.window_timer;
            if (remaining > 0.0f && scale < 1.0f) {
                // Adjust window_start backward so the song-time-derived
                // elapsed time is naturally larger on subsequent ticks,
                // causing the Active phase to expire earlier.  Mutating
                // window_timer directly would be overwritten by
                // shape_window_system on the next frame.
                p_window.window_start -= remaining * (1.0f - scale);
            }
            p_window.window_scale = scale;
            p_window.graded = true;
        }
    };

    const Rectangle player_hitbox = centered_hitbox_rect(player_x);
    auto hitboxes_overlap = [player_hitbox](float obstacle_x) -> bool {
        return CheckCollisionRecs(player_hitbox, centered_hitbox_rect(obstacle_x));
    };

    // Per-kind structural views — each loop touches only entities that actually
    // carry the required components, eliminating per-entity try_get branches.

    // LaneBlock: BlockedLanes only (no RequiredShape)
    {
        auto view = reg.view<ObstacleTag, WorldTransform, BlockedLanes>(
            entt::exclude<ScoredTag, RequiredShape>);
        for (auto [e, wt, blocked] : view.each()) {
            resolve(e, wt.position.y, !((blocked.mask >> p_lane.current) & 1));
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

    if (can_grade_shape) {
        // ShapeGate: RequiredShape only (no BlockedLanes, no RequiredLane)
        {
            auto rhythm_view = reg.view<ObstacleTag, WorldTransform, RequiredShape, BeatInfo>(
                entt::exclude<ScoredTag, BlockedLanes, RequiredLane>);
            for (auto [e, wt, req, info] : rhythm_view.each()) {
                bool lane_match  = hitboxes_overlap(wt.position.x);
                if (!lane_match) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool shape_match = player_matches_required_shape(p_shape, p_window, req.shape);
                bool cleared = shape_match;
                if (cleared) grade_shape_timing(e, info.arrival_time);
                resolve(e, wt.position.y, cleared);
            }

            auto view = reg.view<ObstacleTag, WorldTransform, RequiredShape>(
                entt::exclude<ScoredTag, BlockedLanes, RequiredLane, BeatInfo>);
            for (auto [e, wt, req] : view.each()) {
                bool lane_match  = hitboxes_overlap(wt.position.x);
                if (!lane_match) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool shape_match = player_matches_required_shape(p_shape, p_window, req.shape);
                resolve(e, wt.position.y, shape_match);
            }
        }

        // ComboGate: RequiredShape + BlockedLanes (no RequiredLane)
        {
            auto rhythm_view = reg.view<ObstacleTag, WorldTransform, RequiredShape, BlockedLanes, BeatInfo>(
                entt::exclude<ScoredTag, RequiredLane>);
            for (auto [e, wt, req, blocked, info] : rhythm_view.each()) {
                bool lane_ok  = !((blocked.mask >> p_lane.current) & 1);
                if (!lane_ok) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape);
                bool cleared = shape_ok;
                if (cleared) grade_shape_timing(e, info.arrival_time);
                resolve(e, wt.position.y, cleared);
            }

            auto view = reg.view<ObstacleTag, WorldTransform, RequiredShape, BlockedLanes>(
                entt::exclude<ScoredTag, RequiredLane, BeatInfo>);
            for (auto [e, wt, req, blocked] : view.each()) {
                bool lane_ok  = !((blocked.mask >> p_lane.current) & 1);
                if (!lane_ok) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape);
                resolve(e, wt.position.y, shape_ok);
            }
        }

        // SplitPath: RequiredShape + RequiredLane
        {
            auto rhythm_view = reg.view<ObstacleTag, WorldTransform, RequiredShape, RequiredLane, BeatInfo>(
                entt::exclude<ScoredTag>);
            for (auto [e, wt, req, rlane, info] : rhythm_view.each()) {
                bool lane_ok  = (p_lane.current == rlane.lane);
                if (!lane_ok) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape);
                bool cleared = shape_ok;
                if (cleared) grade_shape_timing(e, info.arrival_time);
                resolve(e, wt.position.y, cleared);
            }

            auto view = reg.view<ObstacleTag, WorldTransform, RequiredShape, RequiredLane>(
                entt::exclude<ScoredTag, BeatInfo>);
            for (auto [e, wt, req, rlane] : view.each()) {
                bool lane_ok  = (p_lane.current == rlane.lane);
                if (!lane_ok) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape);
                resolve(e, wt.position.y, shape_ok);
            }
        }
    } else {
        // ShapeGate: RequiredShape only (no BlockedLanes, no RequiredLane)
        {
            auto view = reg.view<ObstacleTag, WorldTransform, RequiredShape>(
                entt::exclude<ScoredTag, BlockedLanes, RequiredLane>);
            for (auto [e, wt, req] : view.each()) {
                bool lane_match  = hitboxes_overlap(wt.position.x);
                if (!lane_match) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool shape_match = player_matches_required_shape(p_shape, p_window, req.shape);
                resolve(e, wt.position.y, shape_match);
            }
        }

        // ComboGate: RequiredShape + BlockedLanes (no RequiredLane)
        {
            auto view = reg.view<ObstacleTag, WorldTransform, RequiredShape, BlockedLanes>(
                entt::exclude<ScoredTag, RequiredLane>);
            for (auto [e, wt, req, blocked] : view.each()) {
                bool lane_ok  = !((blocked.mask >> p_lane.current) & 1);
                if (!lane_ok) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape);
                resolve(e, wt.position.y, shape_ok);
            }
        }

        // SplitPath: RequiredShape + RequiredLane
        {
            auto view = reg.view<ObstacleTag, WorldTransform, RequiredShape, RequiredLane>(
                entt::exclude<ScoredTag>);
            for (auto [e, wt, req, rlane] : view.each()) {
                bool lane_ok  = (p_lane.current == rlane.lane);
                if (!lane_ok) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape);
                resolve(e, wt.position.y, shape_ok);
            }
        }
    }

}
