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
#include <cmath>

void collision_system(entt::registry& reg, float /*dt*/) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto player_view = reg.view<PlayerTag, Position, PlayerShape, ShapeWindow, Lane, VerticalState>();
    if (player_view.begin() == player_view.end()) return;

    auto player_it = player_view.begin();
    auto [p_pos, p_shape, p_window, p_lane, p_vstate] =
        player_view.get<Position, PlayerShape, ShapeWindow, Lane, VerticalState>(*player_it);

    constexpr float COLLISION_MARGIN = 40.0f;

    auto* song    = reg.ctx().find<SongState>();
    auto* results = reg.ctx().find<SongResults>();
    bool rhythm_mode = (song != nullptr);

    auto resolve = [&](entt::entity entity, const Position& obs_pos, bool cleared) {
        float dist = std::abs(p_pos.y - obs_pos.y + p_vstate.y_offset);
        if (dist > COLLISION_MARGIN) return;

        if (cleared) {
            // In rhythm mode, compute timing grade
            if (rhythm_mode) {
                auto window_phase = static_cast<WindowPhase>(p_window.phase_raw);
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
            // MISS — drain energy and record death cause
            if (results) results->miss_count++;
            auto* energy = reg.ctx().find<EnergyState>();
            if (energy) {
                energy->energy -= constants::ENERGY_DRAIN_MISS;
                if (energy->energy < 1e-6f) energy->energy = 0.0f;
                energy->flash_timer = constants::ENERGY_FLASH_DURATION;
            }
            // Tag the cause of death for the UI (does not override a prior cause)
            if (auto* gos = reg.ctx().find<GameOverState>()) {
                if (gos->cause == DeathCause::None) {
                    auto* obs_comp = reg.try_get<Obstacle>(entity);
                    bool is_bar = obs_comp && (obs_comp->kind == ObstacleKind::LowBar ||
                                              obs_comp->kind == ObstacleKind::HighBar);
                    gos->cause = is_bar ? DeathCause::HitABar : DeathCause::MissedABeat;
                }
            }
            reg.emplace<MissTag>(entity);
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
            case ObstacleKind::LanePushLeft:
            case ObstacleKind::LanePushRight: {
                // Passive: only trigger within the collision window (same
                // distance gate as resolve()).  Scores the obstacle and
                // auto-pushes the player when in the same lane.
                float y_dist = std::abs(p_pos.y - pos.y + p_vstate.y_offset);
                if (y_dist > COLLISION_MARGIN) break;
                bool on_same_lane = (std::abs(p_pos.x - pos.x) < constants::PLAYER_SIZE);
                if (on_same_lane && p_lane.target < 0) {
                    int8_t delta = (obs.kind == ObstacleKind::LanePushLeft) ? -1 : 1;
                    int8_t dest = static_cast<int8_t>(p_lane.current + delta);
                    if (dest >= 0 && dest < constants::LANE_COUNT) {
                        p_lane.target = dest;
                        p_lane.lerp_t = 0.0f;
                    }
                }
                reg.emplace<ScoredTag>(e);
                break;
            }
        }
    }
}
