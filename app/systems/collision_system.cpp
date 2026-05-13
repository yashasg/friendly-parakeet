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
#include <raylib.h>
#include <cmath>

namespace {

Rectangle centered_hitbox_rect(float x) {
    constexpr float kHitboxEdgePadding = 1.0e-4f;
    const float size = constants::PLAYER_SIZE + kHitboxEdgePadding;
    return {x - size * 0.5f, 0.0f, size, 1.0f};
}

bool player_matches_required_shape(const PlayerShape& p_shape,
                                    const ShapeWindow& p_window,
                                    Shape required,
                                    bool rhythm_mode) {
    if (required == Shape::Hexagon) {
        return false;
    }

    if (rhythm_mode && p_window.phase != WindowPhase::Active) {
        return false;
    }

    if (p_shape.current == required) {
        return true;
    }

    return p_window.phase != WindowPhase::Idle && p_window.target_shape == required;
}

bool shape_gate_lane_match(float obstacle_x,
                           float player_x,
                           const Lane& lane) {
    if (CheckCollisionRecs(centered_hitbox_rect(player_x),
                           centered_hitbox_rect(obstacle_x))) {
        return true;
    }

    if (lane.target < 0 || lane.target >= constants::LANE_COUNT) {
        return false;
    }

    return CheckCollisionRecs(centered_hitbox_rect(constants::LANE_X[lane.target]),
                              centered_hitbox_rect(obstacle_x));
}

}  // namespace

void collision_system(entt::registry& reg, float /*dt*/) {
    auto player_view = reg.view<PlayerTag, WorldTransform, PlayerShape, ShapeWindow, Lane, VerticalState>();
    if (player_view.begin() == player_view.end()) return;

    auto player_entity = *player_view.begin();
    auto [p_transform, p_shape, p_window, p_lane, p_vstate] =
        player_view.get<WorldTransform, PlayerShape, ShapeWindow, Lane, VerticalState>(player_entity);

    auto& song = reg.ctx().get<SongState>();
    const bool rhythm_mode = song.playing;

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
            reg.get_or_emplace<MissTag>(entity);
            reg.get_or_emplace<ScoredTag>(entity);
            return;
        }

        reg.get_or_emplace<ScoredTag>(entity);
    };

    const bool can_grade_shape =
        song.half_window > 0.0f && p_window.press_time >= 0.0f;
    auto grade_shape_timing = [&](entt::entity entity, float arrival_time) {
        float delta_seconds = std::abs(p_window.press_time - arrival_time);
        TimingTier tier = compute_timing_tier_from_delta(delta_seconds);
        float precision = 1.0f - (delta_seconds / kTimingOkSeconds);
        if (precision < 0.0f) precision = 0.0f;
        if (precision > 1.0f) precision = 1.0f;
        reg.get_or_emplace<TimingGrade>(entity) = TimingGrade{tier, precision};

        if (!p_window.graded) {
            float scale = window_scale_for_tier(tier);
            const float active_elapsed = song.song_time - p_window.window_start;
            float remaining = song.window_duration - active_elapsed;
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

    // Per-kind structural views — each loop touches only entities that actually
    // carry the required components, eliminating per-entity try_get branches.

    // LaneBlock: BlockedLanes only (no RequiredShape)
    {
        auto view = reg.view<ObstacleTag, Obstacle, WorldTransform, BlockedLanes>(
            entt::exclude<ScoredTag, ResolvedObstacleTag, RequiredShape>);
        for (auto [e, obstacle, wt, blocked] : view.each()) {
            (void)obstacle;
            resolve(e, wt.position.y, !((blocked.mask >> p_lane.current) & 1));
        }
    }


    if (can_grade_shape) {
        // ShapeGate: RequiredShape only (no BlockedLanes, no RequiredLane)
        {
            auto rhythm_view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape, BeatInfo>(
                entt::exclude<ScoredTag, ResolvedObstacleTag, BlockedLanes, RequiredLane>);
            for (auto [e, obstacle, wt, req, info] : rhythm_view.each()) {
                (void)obstacle;
                bool shape_match = player_matches_required_shape(p_shape, p_window, req.shape, rhythm_mode);
                bool lane_match = shape_gate_lane_match(wt.position.x, player_x, p_lane);
                if (!lane_match) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool cleared = shape_match;
                if (cleared) grade_shape_timing(e, info.arrival_time);
                resolve(e, wt.position.y, cleared);
            }

            auto view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape>(
                entt::exclude<ScoredTag, ResolvedObstacleTag, BlockedLanes, RequiredLane, BeatInfo>);
            for (auto [e, obstacle, wt, req] : view.each()) {
                (void)obstacle;
                bool shape_match = player_matches_required_shape(p_shape, p_window, req.shape, rhythm_mode);
                bool lane_match = shape_gate_lane_match(wt.position.x, player_x, p_lane);
                if (!lane_match) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                resolve(e, wt.position.y, shape_match);
            }
        }

        // ComboGate: RequiredShape + BlockedLanes (no RequiredLane)
        {
            auto rhythm_view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape, BlockedLanes, BeatInfo>(
                entt::exclude<ScoredTag, ResolvedObstacleTag, RequiredLane>);
            for (auto [e, obstacle, wt, req, blocked, info] : rhythm_view.each()) {
                (void)obstacle;
                bool lane_ok  = !((blocked.mask >> p_lane.current) & 1);
                if (!lane_ok) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape, rhythm_mode);
                bool cleared = shape_ok;
                if (cleared) grade_shape_timing(e, info.arrival_time);
                resolve(e, wt.position.y, cleared);
            }

            auto view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape, BlockedLanes>(
                entt::exclude<ScoredTag, ResolvedObstacleTag, RequiredLane, BeatInfo>);
            for (auto [e, obstacle, wt, req, blocked] : view.each()) {
                (void)obstacle;
                bool lane_ok  = !((blocked.mask >> p_lane.current) & 1);
                if (!lane_ok) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape, rhythm_mode);
                resolve(e, wt.position.y, shape_ok);
            }
        }

        // SplitPath: RequiredShape + RequiredLane
        {
            auto rhythm_view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape, RequiredLane, BeatInfo>(
                entt::exclude<ScoredTag, ResolvedObstacleTag>);
            for (auto [e, obstacle, wt, req, rlane, info] : rhythm_view.each()) {
                (void)obstacle;
                bool lane_ok  = (p_lane.current == rlane.lane);
                if (!lane_ok) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape, rhythm_mode);
                bool cleared = shape_ok;
                if (cleared) grade_shape_timing(e, info.arrival_time);
                resolve(e, wt.position.y, cleared);
            }

            auto view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape, RequiredLane>(
                entt::exclude<ScoredTag, ResolvedObstacleTag, BeatInfo>);
            for (auto [e, obstacle, wt, req, rlane] : view.each()) {
                (void)obstacle;
                bool lane_ok  = (p_lane.current == rlane.lane);
                if (!lane_ok) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape, rhythm_mode);
                resolve(e, wt.position.y, shape_ok);
            }
        }
    } else {
        // ShapeGate: RequiredShape only (no BlockedLanes, no RequiredLane)
        {
            auto view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape>(
                entt::exclude<ScoredTag, ResolvedObstacleTag, BlockedLanes, RequiredLane>);
            for (auto [e, obstacle, wt, req] : view.each()) {
                (void)obstacle;
                bool shape_match = player_matches_required_shape(p_shape, p_window, req.shape, rhythm_mode);
                bool lane_match = shape_gate_lane_match(wt.position.x, player_x, p_lane);
                if (!lane_match) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                resolve(e, wt.position.y, shape_match);
            }
        }

        // ComboGate: RequiredShape + BlockedLanes (no RequiredLane)
        {
            auto view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape, BlockedLanes>(
                entt::exclude<ScoredTag, ResolvedObstacleTag, RequiredLane>);
            for (auto [e, obstacle, wt, req, blocked] : view.each()) {
                (void)obstacle;
                bool lane_ok  = !((blocked.mask >> p_lane.current) & 1);
                if (!lane_ok) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape, rhythm_mode);
                resolve(e, wt.position.y, shape_ok);
            }
        }

        // SplitPath: RequiredShape + RequiredLane
        {
            auto view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape, RequiredLane>(
                entt::exclude<ScoredTag, ResolvedObstacleTag>);
            for (auto [e, obstacle, wt, req, rlane] : view.each()) {
                (void)obstacle;
                bool lane_ok  = (p_lane.current == rlane.lane);
                if (!lane_ok) {
                    resolve(e, wt.position.y, false);
                    continue;
                }
                bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape, rhythm_mode);
                resolve(e, wt.position.y, shape_ok);
            }
        }
    }

}
