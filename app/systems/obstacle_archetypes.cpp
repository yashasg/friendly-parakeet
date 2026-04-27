#include "obstacle_archetypes.h"
#include "../components/obstacle_data.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../constants.h"

void apply_obstacle_archetype(entt::registry&               reg,
                               entt::entity                 entity,
                               const ObstacleArchetypeInput& in) {
    switch (in.kind) {
        case ObstacleKind::ShapeGate: {
            reg.emplace<Position>(entity, in.x, in.y);
            reg.emplace<Obstacle>(entity, ObstacleKind::ShapeGate, int16_t{constants::PTS_SHAPE_GATE});
            reg.emplace<RequiredShape>(entity, in.shape);
            reg.emplace<DrawSize>(entity, constants::SCREEN_W_F, 80.0f);
            if (in.shape == Shape::Circle)
                reg.emplace<Color>(entity, Color{80, 200, 255, 255});
            else if (in.shape == Shape::Square)
                reg.emplace<Color>(entity, Color{255, 100, 100, 255});
            else
                reg.emplace<Color>(entity, Color{100, 255, 100, 255});
            break;
        }
        case ObstacleKind::LaneBlock: {
            reg.emplace<Position>(entity, in.x, in.y);
            reg.emplace<Obstacle>(entity, ObstacleKind::LaneBlock, int16_t{constants::PTS_LANE_BLOCK});
            reg.emplace<BlockedLanes>(entity, in.mask);
            reg.emplace<DrawSize>(entity, static_cast<float>(constants::SCREEN_W / 3), 80.0f);
            reg.emplace<Color>(entity, Color{255, 60, 60, 255});
            break;
        }
        case ObstacleKind::LowBar: {
            reg.emplace<Position>(entity, in.x, in.y);
            reg.emplace<Obstacle>(entity, ObstacleKind::LowBar, int16_t{constants::PTS_LOW_BAR});
            reg.emplace<RequiredVAction>(entity, VMode::Jumping);
            reg.emplace<DrawSize>(entity, constants::SCREEN_W_F, 40.0f);
            reg.emplace<Color>(entity, Color{255, 180, 0, 255});
            break;
        }
        case ObstacleKind::HighBar: {
            reg.emplace<Position>(entity, in.x, in.y);
            reg.emplace<Obstacle>(entity, ObstacleKind::HighBar, int16_t{constants::PTS_HIGH_BAR});
            reg.emplace<RequiredVAction>(entity, VMode::Sliding);
            reg.emplace<DrawSize>(entity, constants::SCREEN_W_F, 40.0f);
            reg.emplace<Color>(entity, Color{180, 0, 255, 255});
            break;
        }
        case ObstacleKind::ComboGate: {
            reg.emplace<Position>(entity, in.x, in.y);
            reg.emplace<Obstacle>(entity, ObstacleKind::ComboGate, int16_t{constants::PTS_COMBO_GATE});
            reg.emplace<RequiredShape>(entity, in.shape);
            reg.emplace<BlockedLanes>(entity, in.mask);
            reg.emplace<DrawSize>(entity, constants::SCREEN_W_F, 80.0f);
            reg.emplace<Color>(entity, Color{200, 100, 255, 255});
            break;
        }
        case ObstacleKind::SplitPath: {
            reg.emplace<Position>(entity, in.x, in.y);
            reg.emplace<Obstacle>(entity, ObstacleKind::SplitPath, int16_t{constants::PTS_SPLIT_PATH});
            reg.emplace<RequiredShape>(entity, in.shape);
            reg.emplace<RequiredLane>(entity, in.req_lane);
            reg.emplace<DrawSize>(entity, constants::SCREEN_W_F, 80.0f);
            reg.emplace<Color>(entity, Color{255, 215, 0, 255});
            break;
        }
        case ObstacleKind::LanePushLeft:
        case ObstacleKind::LanePushRight: {
            reg.emplace<Position>(entity, in.x, in.y);
            reg.emplace<Obstacle>(entity, in.kind, int16_t{constants::PTS_LANE_PUSH});
            reg.emplace<DrawSize>(entity, static_cast<float>(constants::SCREEN_W / 3), 60.0f);
            reg.emplace<Color>(entity, Color{255, 138, 101, 255});
            break;
        }
    }
}
