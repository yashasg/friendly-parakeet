#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../util/rhythm_math.h"
#include "../components/song_state.h"
#include "gameplay_intents.h"
#include "../constants.h"
#include "../util/lane_utils.h"
#include <cmath>

namespace {

bool player_matches_required_shape(const PlayerShape& p_shape,
                                    const ShapeWindow& p_window,
                                    Shape required,
                                    bool rhythm_mode) {
    if (required == Shape::Hexagon) {
        return false;
    }

    if (rhythm_mode) {
        switch (p_window.phase) {
            case WindowPhase::MorphIn:
                return p_window.press_time >= 0.0f && p_window.target_shape == required;
            case WindowPhase::Active:
                return p_shape.current == required || p_window.target_shape == required;
            case WindowPhase::Idle:
            case WindowPhase::MorphOut:
                return false;
        }
        return false;
    }

    if (p_shape.current == required) {
        return true;
    }

    return p_window.phase != WindowPhase::Idle && p_window.target_shape == required;
}

bool shape_gate_lane_match(int8_t obstacle_lane, int player_lane) {
    return static_cast<int>(obstacle_lane) == player_lane;
}

}  // namespace

void collision_system(entt::registry& reg, [[maybe_unused]] float dt) {
    auto player_view = reg.view<PlayerTag, WorldTransform, PlayerShape, ShapeWindow, Lane, VerticalState>();
    if (player_view.begin() == player_view.end()) return;

    auto player_entity = *player_view.begin();
    auto [p_transform, p_shape, p_window, p_lane, p_vstate] =
        player_view.get<WorldTransform, PlayerShape, ShapeWindow, Lane, VerticalState>(player_entity);
    lane_utils::normalize(p_lane, &p_transform);

    auto& song = reg.ctx().get<SongState>();
    const bool rhythm_mode = song.playing || song.finished;

    // Frame-constant precomputes — both values are invariant across all obstacle
    // loops since player transform and vertical state don't change mid-frame.
    // Precomputing here avoids redundant addition + Vector2 construction per obstacle.
    const float player_timing_y = p_transform.position.y + p_vstate.y_offset;
    const int player_lane       = lane_utils::nearest_lane_for_x(p_transform.position.x);

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
            p_window.graded = true;
        }
    };

    auto resolve_shape_obstacle = [&](entt::entity entity,
                                      const WorldTransform& wt,
                                      Shape required_shape,
                                      bool lane_ok,
                                      const BeatInfo* timing_info) {
        if (!lane_ok) {
            resolve(entity, wt.position.y, false);
            return;
        }

        const bool shape_ok =
            player_matches_required_shape(p_shape, p_window, required_shape, rhythm_mode);
        if (shape_ok && timing_info && can_grade_shape) {
            grade_shape_timing(entity, timing_info->arrival_time);
        }
        resolve(entity, wt.position.y, shape_ok);
    };

    // Per-kind structural views — each loop touches only entities that actually
    // carry the required components, eliminating per-entity try_get branches.

    // Legacy LaneBlock fixture: BlockedLanes only (no RequiredShape).
    // Active beatmaps and obstacle factories reject this kind.
    {
        auto view = reg.view<ObstacleTag, Obstacle, WorldTransform, uint8_t>(
            entt::exclude<ScoredTag, ResolvedObstacleTag, RequiredShape>);
        for (auto [e, obstacle, wt, blocked] : view.each()) {
            (void)obstacle;
            resolve(e, wt.position.y, !((blocked >> player_lane) & 1));
        }
    }


    // ShapeGate: RequiredShape only (no BlockedLanes, no RequiredLane)
    {
        auto rhythm_view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape, ShapeGateLane, BeatInfo>(
            entt::exclude<ScoredTag, ResolvedObstacleTag, uint8_t, RequiredLane>);
        for (auto [e, obstacle, wt, req, lane, info] : rhythm_view.each()) {
            (void)obstacle;
            const bool lane_ok = shape_gate_lane_match(lane.lane, player_lane);
            resolve_shape_obstacle(e, wt, req.shape, lane_ok, &info);
        }

        auto view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape, ShapeGateLane>(
            entt::exclude<ScoredTag, ResolvedObstacleTag, uint8_t, RequiredLane, BeatInfo>);
        for (auto [e, obstacle, wt, req, lane] : view.each()) {
            (void)obstacle;
            const bool lane_ok = shape_gate_lane_match(lane.lane, player_lane);
            resolve_shape_obstacle(e, wt, req.shape, lane_ok, nullptr);
        }

        auto fallback_rhythm_view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape, BeatInfo>(
            entt::exclude<ScoredTag, ResolvedObstacleTag, uint8_t, RequiredLane, ShapeGateLane>);
        for (auto [e, obstacle, wt, req, info] : fallback_rhythm_view.each()) {
            (void)obstacle;
            const bool lane_ok = lane_utils::nearest_lane_for_x(wt.position.x) == player_lane;
            resolve_shape_obstacle(e, wt, req.shape, lane_ok, &info);
        }

        auto fallback_view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape>(
            entt::exclude<ScoredTag, ResolvedObstacleTag, uint8_t, RequiredLane, BeatInfo, ShapeGateLane>);
        for (auto [e, obstacle, wt, req] : fallback_view.each()) {
            (void)obstacle;
            const bool lane_ok = lane_utils::nearest_lane_for_x(wt.position.x) == player_lane;
            resolve_shape_obstacle(e, wt, req.shape, lane_ok, nullptr);
        }
    }

    // Legacy ComboGate fixture: RequiredShape + BlockedLanes (no RequiredLane).
    // Active beatmaps and obstacle factories reject this kind.
    {
        auto rhythm_view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape, uint8_t, BeatInfo>(
            entt::exclude<ScoredTag, ResolvedObstacleTag, RequiredLane>);
        for (auto [e, obstacle, wt, req, blocked, info] : rhythm_view.each()) {
            (void)obstacle;
            const bool lane_ok = !((blocked >> player_lane) & 1);
            resolve_shape_obstacle(e, wt, req.shape, lane_ok, &info);
        }

        auto view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape, uint8_t>(
            entt::exclude<ScoredTag, ResolvedObstacleTag, RequiredLane, BeatInfo>);
        for (auto [e, obstacle, wt, req, blocked] : view.each()) {
            (void)obstacle;
            const bool lane_ok = !((blocked >> player_lane) & 1);
            resolve_shape_obstacle(e, wt, req.shape, lane_ok, nullptr);
        }
    }

    // SplitPath: RequiredShape + RequiredLane
    {
        auto rhythm_view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape, RequiredLane, BeatInfo>(
            entt::exclude<ScoredTag, ResolvedObstacleTag>);
        for (auto [e, obstacle, wt, req, rlane, info] : rhythm_view.each()) {
            (void)obstacle;
            const bool lane_ok = player_lane == rlane.lane;
            resolve_shape_obstacle(e, wt, req.shape, lane_ok, &info);
        }

        auto view = reg.view<ObstacleTag, Obstacle, WorldTransform, RequiredShape, RequiredLane>(
            entt::exclude<ScoredTag, ResolvedObstacleTag, BeatInfo>);
        for (auto [e, obstacle, wt, req, rlane] : view.each()) {
            (void)obstacle;
            const bool lane_ok = player_lane == rlane.lane;
            resolve_shape_obstacle(e, wt, req.shape, lane_ok, nullptr);
        }
    }

}
