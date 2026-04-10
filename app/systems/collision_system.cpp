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

    auto player_view = reg.view<PlayerTag, Position, PlayerShape, Lane, VerticalState>();
    if (player_view.size_hint() == 0) return;

    auto player_it = player_view.begin();
    auto [p_pos, p_shape, p_lane, p_vstate] =
        player_view.get<Position, PlayerShape, Lane, VerticalState>(*player_it);

    constexpr float COLLISION_MARGIN = 40.0f;
    bool game_over = false;

    auto* song    = reg.ctx().find<SongState>();
    auto* hp      = reg.ctx().find<HPState>();
    auto* results = reg.ctx().find<SongResults>();
    bool rhythm_mode = song && song->playing;

    auto resolve = [&](entt::entity entity, const Position& obs_pos, bool cleared) {
        if (game_over) return;
        float dist = std::abs(p_pos.y - obs_pos.y + p_vstate.y_offset);
        if (dist > COLLISION_MARGIN) return;

        if (cleared) {
            // In rhythm mode, compute timing grade
            if (rhythm_mode) {
                auto window_phase = static_cast<WindowPhase>(p_shape.phase_raw);
                if (window_phase == WindowPhase::Active && song->half_window > 0.0f) {
                    float pct_from_peak = std::abs(song->song_time - p_shape.peak_time) / song->half_window;
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

                    // HP recovery on perfect
                    if (tier == TimingTier::Perfect && hp) {
                        hp->current += constants::HP_RECOVER_ON_PERFECT;
                        if (hp->current > hp->max_hp) hp->current = hp->max_hp;
                    }

                    // Window scaling: shorten remaining active time for GOOD/PERFECT
                    if (!p_shape.graded) {
                        float scale = window_scale_for_tier(tier);
                        if (scale < 1.0f) {
                            float remaining = song->window_duration - p_shape.window_timer;
                            if (remaining > 0.0f) {
                                p_shape.window_timer += remaining * (1.0f - scale);
                            }
                        }
                        p_shape.window_scale = scale;
                        p_shape.graded = true;
                    }
                }
            }
            reg.emplace<ScoredTag>(entity);
        } else {
            // MISS
            if (rhythm_mode && hp) {
                hp->current -= constants::HP_DRAIN_ON_MISS;
                if (results) results->miss_count++;
                // Don't instant game-over; hp_system handles it
            } else {
                auto& gs = reg.ctx().get<GameState>();
                gs.transition_pending = true;
                gs.next_phase = GamePhase::GameOver;
                game_over = true;
            }
        }
    };

    // ShapeGate: Hexagon always fails
    for (auto [e, pos, req] :
         reg.view<ObstacleTag, Position, RequiredShape>(
             entt::exclude<ScoredTag, BlockedLanes, RequiredLane, RequiredVAction>).each()) {
        bool shape_match = (p_shape.current == req.shape) && (p_shape.current != Shape::Hexagon);
        resolve(e, pos, shape_match);
    }

    // LaneBlock
    for (auto [e, pos, blocked] :
         reg.view<ObstacleTag, Position, BlockedLanes>(
             entt::exclude<ScoredTag, RequiredShape>).each()) {
        resolve(e, pos, !((blocked.mask >> p_lane.current) & 1));
    }

    // LowBar / HighBar
    for (auto [e, pos, req_v] :
         reg.view<ObstacleTag, Position, RequiredVAction>(
             entt::exclude<ScoredTag>).each()) {
        resolve(e, pos, p_vstate.mode == req_v.action);
    }

    // ComboGate
    for (auto [e, pos, req, blocked] :
         reg.view<ObstacleTag, Position, RequiredShape, BlockedLanes>(
             entt::exclude<ScoredTag, RequiredLane>).each()) {
        bool shape_ok = (p_shape.current == req.shape) && (p_shape.current != Shape::Hexagon);
        bool lane_ok  = !((blocked.mask >> p_lane.current) & 1);
        resolve(e, pos, shape_ok && lane_ok);
    }

    // SplitPath
    for (auto [e, pos, req, rlane] :
         reg.view<ObstacleTag, Position, RequiredShape, RequiredLane>(
             entt::exclude<ScoredTag>).each()) {
        bool shape_ok = (p_shape.current == req.shape) && (p_shape.current != Shape::Hexagon);
        bool lane_ok  = (p_lane.current == rlane.lane);
        resolve(e, pos, shape_ok && lane_ok);
    }
}
