#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/difficulty.h"
#include "../components/player.h"
#include "../constants.h"
#include <cstdlib>

void obstacle_spawn_system(entt::registry& reg, float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto& config = reg.ctx().get<DifficultyConfig>();
    config.spawn_timer -= dt;
    if (config.spawn_timer > 0.0f) return;

    config.spawn_timer = config.spawn_interval;

    // Determine which obstacle kinds are available based on elapsed time
    float t = config.elapsed;
    int max_kind = 0;
    if (t >= 30.0f)  max_kind = 1; // + LaneBlock
    if (t >= 45.0f)  max_kind = 2; // + LowBar
    if (t >= 60.0f)  max_kind = 3; // + HighBar
    if (t >= 90.0f)  max_kind = 4; // + ComboGate
    if (t >= 120.0f) max_kind = 5; // + SplitPath

    int kind_i = std::rand() % (max_kind + 1);
    auto kind  = static_cast<ObstacleKind>(kind_i);

    auto obstacle = reg.create();
    reg.emplace<ObstacleTag>(obstacle);
    reg.emplace<Velocity>(obstacle, 0.0f, config.scroll_speed);
    reg.emplace<DrawLayer>(obstacle, Layer::Game);

    int lane = std::rand() % 3;

    switch (kind) {
        case ObstacleKind::ShapeGate: {
            auto shape = static_cast<Shape>(std::rand() % 3);
            reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::SPAWN_Y);
            reg.emplace<Obstacle>(obstacle, kind, int16_t{constants::PTS_SHAPE_GATE});
            reg.emplace<RequiredShape>(obstacle, shape);
            reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W), 80.0f);

            // DrawColor based on required shape
            if (shape == Shape::Circle)
                reg.emplace<DrawColor>(obstacle, uint8_t{80}, uint8_t{200}, uint8_t{255}, uint8_t{255});
            else if (shape == Shape::Square)
                reg.emplace<DrawColor>(obstacle, uint8_t{255}, uint8_t{100}, uint8_t{100}, uint8_t{255});
            else
                reg.emplace<DrawColor>(obstacle, uint8_t{100}, uint8_t{255}, uint8_t{100}, uint8_t{255});
            break;
        }
        case ObstacleKind::LaneBlock: {
            // Block one lane
            uint8_t mask = uint8_t(1 << lane);
            reg.emplace<Position>(obstacle, constants::LANE_X[lane], constants::SPAWN_Y);
            reg.emplace<Obstacle>(obstacle, kind, int16_t{constants::PTS_LANE_BLOCK});
            reg.emplace<BlockedLanes>(obstacle, mask);
            reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W / 3), 80.0f);
            reg.emplace<DrawColor>(obstacle, uint8_t{255}, uint8_t{60}, uint8_t{60}, uint8_t{255});
            break;
        }
        case ObstacleKind::LowBar: {
            reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::SPAWN_Y);
            reg.emplace<Obstacle>(obstacle, kind, int16_t{constants::PTS_LOW_BAR});
            reg.emplace<RequiredVAction>(obstacle, VMode::Jumping);
            reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W), 40.0f);
            reg.emplace<DrawColor>(obstacle, uint8_t{255}, uint8_t{180}, uint8_t{0}, uint8_t{255});
            break;
        }
        case ObstacleKind::HighBar: {
            reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::SPAWN_Y);
            reg.emplace<Obstacle>(obstacle, kind, int16_t{constants::PTS_HIGH_BAR});
            reg.emplace<RequiredVAction>(obstacle, VMode::Sliding);
            reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W), 40.0f);
            reg.emplace<DrawColor>(obstacle, uint8_t{180}, uint8_t{0}, uint8_t{255}, uint8_t{255});
            break;
        }
        case ObstacleKind::ComboGate: {
            auto shape = static_cast<Shape>(std::rand() % 3);
            uint8_t mask = uint8_t(1 << lane) | uint8_t(1 << ((lane + 1) % 3));
            reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::SPAWN_Y);
            reg.emplace<Obstacle>(obstacle, kind, int16_t{constants::PTS_COMBO_GATE});
            reg.emplace<RequiredShape>(obstacle, shape);
            reg.emplace<BlockedLanes>(obstacle, mask);
            reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W), 80.0f);
            reg.emplace<DrawColor>(obstacle, uint8_t{200}, uint8_t{100}, uint8_t{255}, uint8_t{255});
            break;
        }
        case ObstacleKind::SplitPath: {
            auto shape = static_cast<Shape>(std::rand() % 3);
            reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::SPAWN_Y);
            reg.emplace<Obstacle>(obstacle, kind, int16_t{constants::PTS_SPLIT_PATH});
            reg.emplace<RequiredShape>(obstacle, shape);
            reg.emplace<RequiredLane>(obstacle, int8_t(lane));
            reg.emplace<DrawSize>(obstacle, float(constants::SCREEN_W), 80.0f);
            reg.emplace<DrawColor>(obstacle, uint8_t{255}, uint8_t{215}, uint8_t{0}, uint8_t{255});
            break;
        }
    }
}
