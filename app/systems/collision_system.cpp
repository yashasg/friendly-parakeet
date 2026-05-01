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
    return obstacle_z >= player_timing_point(player_transform, vstate).y;
}

bool player_overlaps_lane(const WorldTransform& player_transform, const Position& obstacle_pos) {
    Rectangle player_lane = centered_rect(player_transform.position.x, 0.0f,
                                          constants::PLAYER_SIZE, 1.0f);
    Rectangle obstacle_lane = centered_rect(obstacle_pos.x, 0.0f, constants::PLAYER_SIZE, 1.0f);
    return CheckCollisionRecs(player_lane, obstacle_lane);
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

        if (!cleared) {
            // MISS — tag only; scoring_system owns energy drain and death-cause attribution.
            reg.emplace<MissTag>(entity);
            reg.emplace<ScoredTag>(entity);
            return;
        }

        // In rhythm mode, compute timing grade from press time for shape-based
        // obstacles. This must not depend on window phase because collision can
        // resolve a frame after input dispatch.
        const bool shape_obstacle = reg.any_of<RequiredShape>(entity);
        if (rhythm_mode && shape_obstacle && song->half_window > 0.0f && p_window.press_time >= 0.0f) {
            // Grade timing by comparing button-press timestamp against
            // the obstacle's scheduled arrival.
            auto* beat_info = reg.try_get<BeatInfo>(entity);
            float reference_time = beat_info ? beat_info->arrival_time
                                             : p_window.press_time;
            float delta_seconds = std::abs(p_window.press_time - reference_time);
            TimingTier tier = compute_timing_tier_from_delta(delta_seconds);
            float precision = 1.0f - (delta_seconds / kTimingOkSeconds);
            if (precision < 0.0f) precision = 0.0f;
            if (precision > 1.0f) precision = 1.0f;
            reg.emplace<TimingGrade>(entity, tier, precision);

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
        }

        reg.emplace<ScoredTag>(entity);
    };

    // Per-kind structural views — each loop touches only entities that actually
    // carry the required components, eliminating per-entity try_get branches.

    // ShapeGate: RequiredShape only (no BlockedLanes, no RequiredLane)
    {
        auto view = reg.view<ObstacleTag, Position, RequiredShape>(
            entt::exclude<ScoredTag, BlockedLanes, RequiredLane>);
        for (auto [e, pos, req] : view.each()) {
            bool shape_match = player_matches_required_shape(p_shape, p_window, req.shape);
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
            bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape);
            bool lane_ok  = !((blocked.mask >> p_lane.current) & 1);
            resolve(e, pos.y, shape_ok && lane_ok);
        }
    }

    // SplitPath: RequiredShape + RequiredLane
    {
        auto view = reg.view<ObstacleTag, Position, RequiredShape, RequiredLane>(
            entt::exclude<ScoredTag>);
        for (auto [e, pos, req, rlane] : view.each()) {
            bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape);
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
