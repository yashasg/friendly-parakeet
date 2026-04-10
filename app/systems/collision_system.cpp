#include "all_systems.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/player.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/game_state.h"
#include "../constants.h"
#include <cmath>

void collision_system(entt::registry& reg, float /*dt*/) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    // Find player
    auto player_view = reg.view<PlayerTag, Position, PlayerShape, Lane, VerticalState>();
    if (player_view.size_hint() == 0) return;

    auto player_it = player_view.begin();
    auto [p_pos, p_shape, p_lane, p_vstate] =
        player_view.get<Position, PlayerShape, Lane, VerticalState>(*player_it);

    constexpr float COLLISION_MARGIN = 40.0f;
    bool game_over = false;

    // Resolve proximity, mark scored or trigger game over.
    auto resolve = [&](entt::entity entity, const Position& obs_pos, bool cleared) {
        if (game_over) return;
        float dist = std::abs(p_pos.y - obs_pos.y + p_vstate.y_offset);
        if (dist > COLLISION_MARGIN) return;
        if (cleared) {
            reg.emplace<ScoredTag>(entity);
        } else {
            auto& gs = reg.ctx().get<GameState>();
            gs.transition_pending = true;
            gs.next_phase = GamePhase::GameOver;
            game_over = true;
        }
    };

    // ShapeGate: RequiredShape only (no BlockedLanes, no RequiredLane, no RequiredVAction)
    for (auto [e, pos, req] :
         reg.view<ObstacleTag, Position, RequiredShape>(
             entt::exclude<ScoredTag, BlockedLanes, RequiredLane, RequiredVAction>).each()) {
        resolve(e, pos, p_shape.current == req.shape);
    }

    // LaneBlock: BlockedLanes only (no RequiredShape)
    for (auto [e, pos, blocked] :
         reg.view<ObstacleTag, Position, BlockedLanes>(
             entt::exclude<ScoredTag, RequiredShape>).each()) {
        resolve(e, pos, !((blocked.mask >> p_lane.current) & 1));
    }

    // LowBar / HighBar: RequiredVAction
    for (auto [e, pos, req_v] :
         reg.view<ObstacleTag, Position, RequiredVAction>(
             entt::exclude<ScoredTag>).each()) {
        resolve(e, pos, p_vstate.mode == req_v.action);
    }

    // ComboGate: RequiredShape + BlockedLanes (no RequiredLane)
    for (auto [e, pos, req, blocked] :
         reg.view<ObstacleTag, Position, RequiredShape, BlockedLanes>(
             entt::exclude<ScoredTag, RequiredLane>).each()) {
        bool shape_ok = (p_shape.current == req.shape);
        bool lane_ok  = !((blocked.mask >> p_lane.current) & 1);
        resolve(e, pos, shape_ok && lane_ok);
    }

    // SplitPath: RequiredShape + RequiredLane
    for (auto [e, pos, req, rlane] :
         reg.view<ObstacleTag, Position, RequiredShape, RequiredLane>(
             entt::exclude<ScoredTag>).each()) {
        bool shape_ok = (p_shape.current == req.shape);
        bool lane_ok  = (p_lane.current == rlane.lane);
        resolve(e, pos, shape_ok && lane_ok);
    }
}
