#include "obstacle_entity.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../gameobjects/shape_obstacle.h"
#include "../constants.h"

entt::entity spawn_obstacle(entt::registry& reg, const ObstacleSpawnParams& params,
                             const BeatInfo* beat_info) {
    auto e = reg.create();
    reg.emplace<Velocity>(e, 0.0f, params.speed);
    reg.emplace<DrawLayer>(e, Layer::Game);

    if (beat_info) {
        reg.emplace<BeatInfo>(e, *beat_info);
    }

    switch (params.kind) {
        case ObstacleKind::ShapeGate: {
            reg.emplace<Position>(e, params.x, params.y);
            reg.emplace<Obstacle>(e, ObstacleKind::ShapeGate, int16_t{constants::PTS_SHAPE_GATE});
            reg.emplace<RequiredShape>(e, params.shape);
            reg.emplace<DrawSize>(e, constants::SCREEN_W_F, 80.0f);
            if (params.shape == Shape::Circle)
                reg.emplace<Color>(e, Color{80, 200, 255, 255});
            else if (params.shape == Shape::Square)
                reg.emplace<Color>(e, Color{255, 100, 100, 255});
            else
                reg.emplace<Color>(e, Color{100, 255, 100, 255});
            break;
        }
        case ObstacleKind::LaneBlock: {
            reg.emplace<Position>(e, params.x, params.y);
            reg.emplace<Obstacle>(e, ObstacleKind::LaneBlock, int16_t{constants::PTS_LANE_BLOCK});
            reg.emplace<BlockedLanes>(e, params.mask);
            reg.emplace<DrawSize>(e, static_cast<float>(constants::SCREEN_W / 3), 80.0f);
            reg.emplace<Color>(e, Color{255, 60, 60, 255});
            break;
        }
        case ObstacleKind::LowBar: {
            reg.emplace<ObstacleScrollZ>(e, params.y);
            reg.emplace<Obstacle>(e, ObstacleKind::LowBar, int16_t{constants::PTS_LOW_BAR});
            reg.emplace<RequiredVAction>(e, VMode::Jumping);
            reg.emplace<DrawSize>(e, constants::SCREEN_W_F, 40.0f);
            reg.emplace<Color>(e, Color{255, 180, 0, 255});
            break;
        }
        case ObstacleKind::HighBar: {
            reg.emplace<ObstacleScrollZ>(e, params.y);
            reg.emplace<Obstacle>(e, ObstacleKind::HighBar, int16_t{constants::PTS_HIGH_BAR});
            reg.emplace<RequiredVAction>(e, VMode::Sliding);
            reg.emplace<DrawSize>(e, constants::SCREEN_W_F, 40.0f);
            reg.emplace<Color>(e, Color{180, 0, 255, 255});
            break;
        }
        case ObstacleKind::ComboGate: {
            reg.emplace<Position>(e, params.x, params.y);
            reg.emplace<Obstacle>(e, ObstacleKind::ComboGate, int16_t{constants::PTS_COMBO_GATE});
            reg.emplace<RequiredShape>(e, params.shape);
            reg.emplace<BlockedLanes>(e, params.mask);
            reg.emplace<DrawSize>(e, constants::SCREEN_W_F, 80.0f);
            reg.emplace<Color>(e, Color{200, 100, 255, 255});
            break;
        }
        case ObstacleKind::SplitPath: {
            reg.emplace<Position>(e, params.x, params.y);
            reg.emplace<Obstacle>(e, ObstacleKind::SplitPath, int16_t{constants::PTS_SPLIT_PATH});
            reg.emplace<RequiredShape>(e, params.shape);
            reg.emplace<RequiredLane>(e, params.req_lane);
            reg.emplace<DrawSize>(e, constants::SCREEN_W_F, 80.0f);
            reg.emplace<Color>(e, Color{255, 215, 0, 255});
            break;
        }
        case ObstacleKind::LanePushLeft:
        case ObstacleKind::LanePushRight: {
            reg.emplace<Position>(e, params.x, params.y);
            reg.emplace<Obstacle>(e, params.kind, int16_t{constants::PTS_LANE_PUSH});
            reg.emplace<DrawSize>(e, static_cast<float>(constants::SCREEN_W / 3), 60.0f);
            reg.emplace<Color>(e, Color{255, 138, 101, 255});
            break;
        }
    }

    spawn_obstacle_meshes(reg, e);
    build_obstacle_model(reg, e);
    reg.emplace<ObstacleTag>(e);

    return e;
}
