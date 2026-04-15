#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../constants.h"
#include <cmath>

void collision_system(entt::registry& reg, float /*dt*/) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto player_view = reg.view<PlayerTag, Position, PlayerShape, ShapeWindow, Lane, VerticalState>();
    if (player_view.size_hint() == 0) return;

    auto player_it = player_view.begin();
    auto [p_pos, p_shape, p_window, p_lane, p_vstate] =
        player_view.get<Position, PlayerShape, ShapeWindow, Lane, VerticalState>(*player_it);

    constexpr float COLLISION_MARGIN = 40.0f;
    bool game_over = false;

    auto* song    = reg.ctx().find<SongState>();
    auto* results = reg.ctx().find<SongResults>();
    bool rhythm_mode = (song != nullptr);

    auto resolve = [&](entt::entity entity, const Position& obs_pos, bool cleared) {
        if (game_over) return;
        float dist = std::abs(p_pos.y - obs_pos.y + p_vstate.y_offset);
        if (dist > COLLISION_MARGIN) return;

        if (cleared) {
            // In rhythm mode, compute timing grade
            if (rhythm_mode) {
                auto window_phase = static_cast<WindowPhase>(p_window.phase_raw);
                if (window_phase == WindowPhase::Active && song->half_window > 0.0f) {
                    // Grade timing against the obstacle's expected beat time,
                    // comparing when the player pressed vs when the obstacle
                    // arrives.  This ensures "perfect" means the player tapped
                    // on the beat they meet the obstacle, not an earlier beat.
                    auto* beat_info = reg.try_get<BeatInfo>(entity);
                    float reference_time = beat_info ? beat_info->arrival_time
                                                     : p_window.peak_time;
                    float pct_from_peak = std::abs(p_window.press_time - reference_time) / song->half_window;
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
            // MISS — instant game over
            if (results) results->miss_count++;
            auto& gs = reg.ctx().get<GameState>();
            gs.transition_pending = true;
            gs.next_phase = GamePhase::GameOver;
            game_over = true;
            reg.emplace<ScoredTag>(entity);
        }
    };

    // Single-pass collision: iterate all unscored obstacles once, dispatch by kind.
    auto obs_view = reg.view<ObstacleTag, Position, Obstacle>(entt::exclude<ScoredTag>);
    for (auto [e, pos, obs] : obs_view.each()) {
        switch (obs.kind) {
            case ObstacleKind::ShapeGate: {
                auto* req = reg.try_get<RequiredShape>(e);
                if (!req) break;
                bool shape_match = (p_shape.current == req->shape) && (p_shape.current != Shape::Hexagon);
                bool lane_match = (std::abs(p_pos.x - pos.x) < constants::PLAYER_SIZE);
                resolve(e, pos, shape_match && lane_match);
                break;
            }
            case ObstacleKind::LaneBlock: {
                auto* blocked = reg.try_get<BlockedLanes>(e);
                if (!blocked) break;
                resolve(e, pos, !((blocked->mask >> p_lane.current) & 1));
                break;
            }
            case ObstacleKind::LowBar:
            case ObstacleKind::HighBar: {
                auto* req_v = reg.try_get<RequiredVAction>(e);
                if (!req_v) break;
                resolve(e, pos, p_vstate.mode == req_v->action);
                break;
            }
            case ObstacleKind::ComboGate: {
                auto* req = reg.try_get<RequiredShape>(e);
                auto* blocked = reg.try_get<BlockedLanes>(e);
                if (!req || !blocked) break;
                bool shape_ok = (p_shape.current == req->shape) && (p_shape.current != Shape::Hexagon);
                bool lane_ok  = !((blocked->mask >> p_lane.current) & 1);
                resolve(e, pos, shape_ok && lane_ok);
                break;
            }
            case ObstacleKind::SplitPath: {
                auto* req = reg.try_get<RequiredShape>(e);
                auto* rlane = reg.try_get<RequiredLane>(e);
                if (!req || !rlane) break;
                bool shape_ok = (p_shape.current == req->shape) && (p_shape.current != Shape::Hexagon);
                bool lane_ok  = (p_lane.current == rlane->lane);
                resolve(e, pos, shape_ok && lane_ok);
                break;
            }
        }
    }
}
