#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../components/song_state.h"
#include "../constants.h"
#include <raylib.h>
#include <cmath>

namespace {

Rectangle centered_rect(float cx, float cy, float w, float h) {
    return {cx - w * 0.5f, cy - h * 0.5f, w, h};
}

Vector2 player_timing_point(const Position& pos, const VerticalState& vstate) {
    return {0.0f, pos.y + vstate.y_offset};
}

bool player_in_timing_window(const Position& player_pos,
                             const VerticalState& vstate,
                             const Position& obstacle_pos) {
    Rectangle timing_window = {
        0.0f,
        obstacle_pos.y - constants::COLLISION_MARGIN,
        1.0f,
        constants::COLLISION_MARGIN * 2.0f
    };
    return CheckCollisionPointRec(player_timing_point(player_pos, vstate), timing_window);
}

bool player_overlaps_lane(const Position& player_pos, const Position& obstacle_pos) {
    Rectangle player_lane = centered_rect(player_pos.x, 0.0f, constants::PLAYER_SIZE, 1.0f);
    Rectangle obstacle_lane = centered_rect(obstacle_pos.x, 0.0f, constants::PLAYER_SIZE, 1.0f);
    return CheckCollisionRecs(player_lane, obstacle_lane);
}

}  // namespace

void collision_system(entt::registry& reg, float /*dt*/) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto player_view = reg.view<PlayerTag, Position, PlayerShape, ShapeWindow, Lane, VerticalState>();
    if (player_view.begin() == player_view.end()) return;

    auto player_it = player_view.begin();
    auto [p_pos, p_shape, p_window, p_lane, p_vstate] =
        player_view.get<Position, PlayerShape, ShapeWindow, Lane, VerticalState>(*player_it);

    auto* song    = reg.ctx().find<SongState>();
    auto* results = reg.ctx().find<SongResults>();
    auto* gos     = reg.ctx().find<GameOverState>();  // #309: hoisted above resolve lambda
    bool rhythm_mode = (song != nullptr);

    // resolve: tag entity as scored (cleared) or missed.
    // kind is passed by the caller — no try_get needed since each per-kind
    // loop already holds the Obstacle component.
    auto resolve = [&](entt::entity entity, const Position& obs_pos, bool cleared,
                       ObstacleKind kind) {
        if (!player_in_timing_window(p_pos, p_vstate, obs_pos)) return;

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
            // MISS — tag and record cause; energy drain handled by scoring_system.
            // Tag the cause of death for the UI (does not override a prior cause)
            if (gos && gos->cause == DeathCause::None) {
                bool is_bar = (kind == ObstacleKind::LowBar || kind == ObstacleKind::HighBar);
                gos->cause = is_bar ? DeathCause::HitABar : DeathCause::MissedABeat;
            }
            reg.emplace<MissTag>(entity);
            reg.emplace<ScoredTag>(entity);
        }
    };

    // Per-kind structural views — each loop touches only entities that actually
    // carry the required components, eliminating per-entity try_get branches.

    // ShapeGate: RequiredShape only (no BlockedLanes, no RequiredLane)
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, RequiredShape>(
            entt::exclude<ScoredTag, BlockedLanes, RequiredLane>);
        for (auto [e, pos, obs, req] : view.each()) {
            bool shape_match = (p_shape.current == req.shape) && (p_shape.current != Shape::Hexagon);
            bool lane_match  = player_overlaps_lane(p_pos, pos);
            resolve(e, pos, shape_match && lane_match, obs.kind);
        }
    }

    // LaneBlock: BlockedLanes only (no RequiredShape)
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, BlockedLanes>(
            entt::exclude<ScoredTag, RequiredShape>);
        for (auto [e, pos, obs, blocked] : view.each()) {
            resolve(e, pos, !((blocked.mask >> p_lane.current) & 1), obs.kind);
        }
    }

    // LowBar / HighBar: RequiredVAction
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, RequiredVAction>(
            entt::exclude<ScoredTag>);
        for (auto [e, pos, obs, req_v] : view.each()) {
            resolve(e, pos, p_vstate.mode == req_v.action, obs.kind);
        }
    }

    // ComboGate: RequiredShape + BlockedLanes (no RequiredLane)
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, RequiredShape, BlockedLanes>(
            entt::exclude<ScoredTag, RequiredLane>);
        for (auto [e, pos, obs, req, blocked] : view.each()) {
            bool shape_ok = (p_shape.current == req.shape) && (p_shape.current != Shape::Hexagon);
            bool lane_ok  = !((blocked.mask >> p_lane.current) & 1);
            resolve(e, pos, shape_ok && lane_ok, obs.kind);
        }
    }

    // SplitPath: RequiredShape + RequiredLane
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, RequiredShape, RequiredLane>(
            entt::exclude<ScoredTag>);
        for (auto [e, pos, obs, req, rlane] : view.each()) {
            bool shape_ok = (p_shape.current == req.shape) && (p_shape.current != Shape::Hexagon);
            bool lane_ok  = (p_lane.current == rlane.lane);
            resolve(e, pos, shape_ok && lane_ok, obs.kind);
        }
    }

    // LanePushLeft / LanePushRight: no data components (structural negative).
    // Passive: auto-pushes player when in the same lane; always scores.
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle>(
            entt::exclude<ScoredTag, RequiredShape, BlockedLanes, RequiredLane, RequiredVAction>);
        for (auto [e, pos, obs] : view.each()) {
            if (!player_in_timing_window(p_pos, p_vstate, pos)) continue;
            bool on_same_lane = player_overlaps_lane(p_pos, pos);
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
