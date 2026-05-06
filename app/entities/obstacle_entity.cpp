#include "obstacle_entity.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/render_tags.h"
#include "obstacle_render_entity.h"
#include "../constants.h"

namespace {

void emplace_gate_obstacle(entt::registry& reg,
                           entt::entity entity,
                           ObstacleKind kind,
                           int16_t points,
                           float width,
                           float height,
                           SDL_Color color) {
    reg.emplace<Obstacle>(entity, kind, points);
    reg.emplace<DrawSize>(entity, width, height);
    reg.emplace<SDL_Color>(entity, color);
}

void emplace_bar_obstacle(entt::registry& reg,
                          entt::entity entity,
                          ObstacleKind kind,
                          int16_t points,
                          VMode required_action,
                          SDL_Color color,
                          float initial_z) {
    reg.emplace<ObstacleScrollZ>(entity, initial_z);
    reg.emplace<Obstacle>(entity, kind, points);
    reg.emplace<RequiredVAction>(entity, required_action);
    reg.emplace<DrawSize>(entity, constants::SCREEN_W_F, 40.0f);
    reg.emplace<SDL_Color>(entity, color);
    reg.emplace<BarObstacleTag>(entity);
}

SDL_Color shape_gate_color(Shape shape) {
    switch (shape) {
        case Shape::Circle:
            return SDL_Color{80, 200, 255, 255};
        case Shape::Square:
            return SDL_Color{255, 100, 100, 255};
        case Shape::Triangle:
        case Shape::Hexagon:
            return SDL_Color{100, 255, 100, 255};
    }
    return SDL_Color{100, 255, 100, 255};
}

}  // namespace

entt::entity spawn_obstacle(entt::registry& reg, const ObstacleSpawnParams& params,
                             const BeatInfo* beat_info) {
    auto e = reg.create();
    reg.emplace<MotionVelocity>(e, MotionVelocity{{0.0f, params.speed}});
    reg.emplace<WorldTransform>(e, WorldTransform{{params.x, params.y}});
    reg.emplace<DrawLayer>(e, Layer::Game);
    reg.emplace<TagWorldPass>(e);

    if (beat_info) {
        reg.emplace<BeatInfo>(e, *beat_info);
    }

    switch (params.kind) {
        case ObstacleKind::ShapeGate: {
            emplace_gate_obstacle(reg,
                                  e,
                                  ObstacleKind::ShapeGate,
                                  int16_t{constants::PTS_SHAPE_GATE},
                                  constants::SCREEN_W_F,
                                  80.0f,
                                  shape_gate_color(params.shape));
            reg.emplace<RequiredShape>(e, params.shape);
            break;
        }
        case ObstacleKind::LaneBlock: {
            emplace_gate_obstacle(reg,
                                  e,
                                  ObstacleKind::LaneBlock,
                                  int16_t{constants::PTS_LANE_BLOCK},
                                  static_cast<float>(constants::SCREEN_W / 3),
                                  80.0f,
                                  SDL_Color{255, 60, 60, 255});
            reg.emplace<BlockedLanes>(e, params.mask);
            break;
        }
        case ObstacleKind::LowBar: {
            emplace_bar_obstacle(reg,
                                 e,
                                 ObstacleKind::LowBar,
                                 constants::PTS_LOW_BAR,
                                 VMode::Jumping,
                                 SDL_Color{255, 180, 0, 255},
                                 params.y);
            break;
        }
        case ObstacleKind::HighBar: {
            emplace_bar_obstacle(reg,
                                 e,
                                 ObstacleKind::HighBar,
                                 constants::PTS_HIGH_BAR,
                                 VMode::Sliding,
                                 SDL_Color{180, 0, 255, 255},
                                 params.y);
            break;
        }
        case ObstacleKind::ComboGate: {
            emplace_gate_obstacle(reg,
                                  e,
                                  ObstacleKind::ComboGate,
                                  int16_t{constants::PTS_COMBO_GATE},
                                  constants::SCREEN_W_F,
                                  80.0f,
                                  SDL_Color{200, 100, 255, 255});
            reg.emplace<RequiredShape>(e, params.shape);
            reg.emplace<BlockedLanes>(e, params.mask);
            break;
        }
        case ObstacleKind::SplitPath: {
            emplace_gate_obstacle(reg,
                                  e,
                                  ObstacleKind::SplitPath,
                                  int16_t{constants::PTS_SPLIT_PATH},
                                  constants::SCREEN_W_F,
                                  80.0f,
                                  SDL_Color{255, 215, 0, 255});
            reg.emplace<RequiredShape>(e, params.shape);
            reg.emplace<RequiredLane>(e, params.req_lane);
            break;
        }
    }

    if (has_mesh_children(params.kind)) {
        spawn_obstacle_meshes(reg, e);
    }
    if (is_bar_obstacle_kind(params.kind)) {
        build_obstacle_model(reg, e);
    }
    reg.emplace<ObstacleTag>(e);

    return e;
}
