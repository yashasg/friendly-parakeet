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

    auto obs_view = reg.view<ObstacleTag, Position, Obstacle>(entt::exclude<ScoredTag>);
    for (auto [entity, obs_pos, obs] : obs_view.each()) {
        float dist = std::abs(p_pos.y - obs_pos.y + p_vstate.y_offset);
        if (dist > COLLISION_MARGIN) continue;

        bool cleared = false;

        switch (obs.kind) {
            case ObstacleKind::ShapeGate: {
                auto* req = reg.try_get<RequiredShape>(entity);
                if (req && p_shape.current == req->shape) {
                    cleared = true;
                }
                break;
            }
            case ObstacleKind::LaneBlock: {
                auto* blocked = reg.try_get<BlockedLanes>(entity);
                if (blocked) {
                    bool lane_blocked = (blocked->mask >> p_lane.current) & 1;
                    if (!lane_blocked) {
                        cleared = true;
                    }
                }
                break;
            }
            case ObstacleKind::LowBar: {
                if (p_vstate.mode == VMode::Jumping) {
                    cleared = true;
                }
                break;
            }
            case ObstacleKind::HighBar: {
                if (p_vstate.mode == VMode::Sliding) {
                    cleared = true;
                }
                break;
            }
            case ObstacleKind::ComboGate: {
                auto* req = reg.try_get<RequiredShape>(entity);
                auto* blocked = reg.try_get<BlockedLanes>(entity);
                bool shape_ok = req && (p_shape.current == req->shape);
                bool lane_ok = blocked && !((blocked->mask >> p_lane.current) & 1);
                if (shape_ok && lane_ok) {
                    cleared = true;
                }
                break;
            }
            case ObstacleKind::SplitPath: {
                auto* req = reg.try_get<RequiredShape>(entity);
                auto* rlane = reg.try_get<RequiredLane>(entity);
                bool shape_ok = req && (p_shape.current == req->shape);
                bool lane_ok = rlane && (p_lane.current == rlane->lane);
                if (shape_ok && lane_ok) {
                    cleared = true;
                }
                break;
            }
        }

        if (cleared) {
            reg.emplace<ScoredTag>(entity);
        } else {
            // Collision → game over
            auto& gs = reg.ctx().get<GameState>();
            gs.transition_pending = true;
            gs.next_phase = GamePhase::GameOver;
            return;
        }
    }
}
