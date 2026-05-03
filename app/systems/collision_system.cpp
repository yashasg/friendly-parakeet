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

    auto* song    = reg.ctx().find<SongState>();
    bool rhythm_mode = (song != nullptr);

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

    // Inline 1D lane-overlap check — equivalent to the two-rect CheckCollisionRecs
    // call (y-axis always overlaps since both rects use y=0,h=1; reduces to x-axis
    // interval: |obs_x - player_x| < PLAYER_SIZE). Saves 2× centered_rect + 1×
    // CheckCollisionRecs per obstacle.
    auto lane_overlaps = [player_x](float obstacle_x) -> bool {
        float dx = obstacle_x - player_x;
        return (dx > -constants::PLAYER_SIZE) && (dx < constants::PLAYER_SIZE);
    };

    // Per-kind structural views — each loop touches only entities that actually
    // carry the required components, eliminating per-entity try_get branches.

    // ShapeGate: RequiredShape only (no BlockedLanes, no RequiredLane)
    {
        auto view = reg.view<ObstacleTag, WorldTransform, RequiredShape>(
            entt::exclude<ScoredTag, BlockedLanes, RequiredLane>);
        for (auto [e, wt, req] : view.each()) {
            bool shape_match = player_matches_required_shape(p_shape, p_window, req.shape);
            bool lane_match  = lane_overlaps(wt.position.x);
            resolve(e, wt.position.y, shape_match && lane_match);
        }
    }

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

    // ComboGate: RequiredShape + BlockedLanes (no RequiredLane)
    {
        auto view = reg.view<ObstacleTag, WorldTransform, RequiredShape, BlockedLanes>(
            entt::exclude<ScoredTag, RequiredLane>);
        for (auto [e, wt, req, blocked] : view.each()) {
            bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape);
            bool lane_ok  = !((blocked.mask >> p_lane.current) & 1);
            resolve(e, wt.position.y, shape_ok && lane_ok);
        }
    }

    // SplitPath: RequiredShape + RequiredLane
    {
        auto view = reg.view<ObstacleTag, WorldTransform, RequiredShape, RequiredLane>(
            entt::exclude<ScoredTag>);
        for (auto [e, wt, req, rlane] : view.each()) {
            bool shape_ok = player_matches_required_shape(p_shape, p_window, req.shape);
            bool lane_ok  = (p_lane.current == rlane.lane);
            resolve(e, wt.position.y, shape_ok && lane_ok);
        }
    }

    // LanePush: positive discriminant via LanePushDelta component.
    // Emplaces PendingLanePush on the player; lane_push_response_system
    // applies it to p_lane in the same frame (after collision, before render).
    {
        auto view = reg.view<ObstacleTag, WorldTransform, LanePushDelta>(entt::exclude<ScoredTag>);
        for (auto [e, wt, lpd] : view.each()) {
            if (wt.position.y < player_timing_y) continue;
            if (lane_overlaps(wt.position.x) && !reg.all_of<PendingLanePush>(player_entity)) {
                reg.emplace<PendingLanePush>(player_entity, lpd.delta);
            }
            reg.emplace<ScoredTag>(e);
        }
    }
}
