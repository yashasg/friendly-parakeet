#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/difficulty.h"
#include "../components/player.h"
#include "../components/rhythm.h"
#include "../components/rng.h"
#include "../gameobjects/shape_obstacle.h"
#include "../constants.h"
#include <random>

void obstacle_spawn_system(entt::registry& reg, float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    // Bypass random spawning when a charted song is playing
    auto* song = reg.ctx().find<SongState>();
    if (song && song->playing) return;

    auto& config = reg.ctx().get<DifficultyConfig>();
    config.spawn_timer -= dt;
    if (config.spawn_timer > 0.0f) return;

    config.spawn_timer = config.spawn_interval;

    if (!reg.ctx().find<RNGState>()) reg.ctx().emplace<RNGState>();
    auto& rng = *reg.ctx().find<RNGState>();

    // Determine which obstacle kinds are available based on elapsed time
    float t = config.elapsed;
    int max_kind = 0;
    if (t >= 30.0f)  max_kind = 1; // + LaneBlock
    if (t >= 45.0f)  max_kind = 2; // + LowBar
    if (t >= 60.0f)  max_kind = 3; // + HighBar
    if (t >= 90.0f)  max_kind = 4; // + ComboGate
    if (t >= 120.0f) max_kind = 5; // + SplitPath

    int kind_i = std::uniform_int_distribution<int>(0, max_kind)(rng.engine);
    auto kind  = static_cast<ObstacleKind>(kind_i);

    auto obstacle = reg.create();
    reg.emplace<ObstacleTag>(obstacle);
    reg.emplace<Velocity>(obstacle, 0.0f, config.scroll_speed);
    reg.emplace<DrawLayer>(obstacle, Layer::Game);

    int lane = std::uniform_int_distribution<int>(0, 2)(rng.engine);

    switch (kind) {
        case ObstacleKind::ShapeGate: {
            auto shape = static_cast<Shape>(std::uniform_int_distribution<int>(0, 2)(rng.engine));
            reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::SPAWN_Y);
            reg.emplace<Obstacle>(obstacle, kind, int16_t{constants::PTS_SHAPE_GATE});
            reg.emplace<RequiredShape>(obstacle, shape);
            reg.emplace<DrawSize>(obstacle, constants::SCREEN_W_F, 80.0f);

            // Color based on required shape
            if (shape == Shape::Circle)
                reg.emplace<Color>(obstacle, Color{80, 200, 255, 255});
            else if (shape == Shape::Square)
                reg.emplace<Color>(obstacle, Color{255, 100, 100, 255});
            else
                reg.emplace<Color>(obstacle, Color{100, 255, 100, 255});
            break;
        }
        case ObstacleKind::LaneBlock: {
            // Convert legacy LaneBlock to LanePush:
            //   Lane 0 (left)   → push right
            //   Lane 2 (right)  → push left
            //   Lane 1 (center) → random
            ObstacleKind push_kind;
            if (lane == 0) {
                push_kind = ObstacleKind::LanePushRight;
            } else if (lane == 2) {
                push_kind = ObstacleKind::LanePushLeft;
            } else {
                push_kind = (std::uniform_int_distribution<int>(0, 1)(rng.engine) == 0)
                    ? ObstacleKind::LanePushLeft
                    : ObstacleKind::LanePushRight;
            }
            reg.emplace<Position>(obstacle, constants::LANE_X[lane], constants::SPAWN_Y);
            reg.emplace<Obstacle>(obstacle, push_kind, int16_t{constants::PTS_LANE_PUSH});
            reg.emplace<DrawSize>(obstacle, static_cast<float>(constants::SCREEN_W / 3), 60.0f);
            reg.emplace<Color>(obstacle, Color{255, 138, 101, 255});
            break;
        }
        case ObstacleKind::LowBar: {
            reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::SPAWN_Y);
            reg.emplace<Obstacle>(obstacle, kind, int16_t{constants::PTS_LOW_BAR});
            reg.emplace<RequiredVAction>(obstacle, VMode::Jumping);
            reg.emplace<DrawSize>(obstacle, constants::SCREEN_W_F, 40.0f);
            reg.emplace<Color>(obstacle, Color{255, 180, 0, 255});
            break;
        }
        case ObstacleKind::HighBar: {
            reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::SPAWN_Y);
            reg.emplace<Obstacle>(obstacle, kind, int16_t{constants::PTS_HIGH_BAR});
            reg.emplace<RequiredVAction>(obstacle, VMode::Sliding);
            reg.emplace<DrawSize>(obstacle, constants::SCREEN_W_F, 40.0f);
            reg.emplace<Color>(obstacle, Color{180, 0, 255, 255});
            break;
        }
        case ObstacleKind::ComboGate: {
            auto shape = static_cast<Shape>(std::uniform_int_distribution<int>(0, 2)(rng.engine));
            uint8_t mask = uint8_t(1 << lane) | uint8_t(1 << ((lane + 1) % 3));
            reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::SPAWN_Y);
            reg.emplace<Obstacle>(obstacle, kind, int16_t{constants::PTS_COMBO_GATE});
            reg.emplace<RequiredShape>(obstacle, shape);
            reg.emplace<BlockedLanes>(obstacle, mask);
            reg.emplace<DrawSize>(obstacle, constants::SCREEN_W_F, 80.0f);
            reg.emplace<Color>(obstacle, Color{200, 100, 255, 255});
            break;
        }
        case ObstacleKind::SplitPath: {
            auto shape = static_cast<Shape>(std::uniform_int_distribution<int>(0, 2)(rng.engine));
            reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::SPAWN_Y);
            reg.emplace<Obstacle>(obstacle, kind, int16_t{constants::PTS_SPLIT_PATH});
            reg.emplace<RequiredShape>(obstacle, shape);
            reg.emplace<RequiredLane>(obstacle, int8_t(lane));
            reg.emplace<DrawSize>(obstacle, constants::SCREEN_W_F, 80.0f);
            reg.emplace<Color>(obstacle, Color{255, 215, 0, 255});
            break;
        }
        case ObstacleKind::LanePushLeft:
        case ObstacleKind::LanePushRight: {
            // Shouldn't reach here from random spawner (converted from LaneBlock above),
            // but handle gracefully.
            reg.emplace<Position>(obstacle, constants::LANE_X[lane], constants::SPAWN_Y);
            reg.emplace<Obstacle>(obstacle, kind, int16_t{constants::PTS_LANE_PUSH});
            reg.emplace<DrawSize>(obstacle, static_cast<float>(constants::SCREEN_W / 3), 60.0f);
            reg.emplace<Color>(obstacle, Color{255, 138, 101, 255});
            break;
        }
    }

    // Spawn visual mesh children for multi-slab obstacle types
    spawn_obstacle_meshes(reg, obstacle);
}
