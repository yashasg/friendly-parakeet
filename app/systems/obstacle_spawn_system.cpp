#include "all_systems.h"
#include "obstacle_archetypes.h"
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

    auto& rng = reg.ctx().get<RNGState>();

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

    // Resolve kind and position.  LaneBlock is a legacy slot in the random pool
    // that maps to a directional LanePush; all other gate/bar kinds centre on
    // lane 1.  LanePushLeft/Right (not reached from random pool) use lane x.
    ObstacleKind resolved_kind = kind;
    float x_pos = constants::LANE_X[1];

    if (kind == ObstacleKind::LaneBlock) {
        // Convert legacy LaneBlock to LanePush:
        //   Lane 0 (left)   → push right
        //   Lane 2 (right)  → push left
        //   Lane 1 (center) → random
        if (lane == 0) {
            resolved_kind = ObstacleKind::LanePushRight;
        } else if (lane == 2) {
            resolved_kind = ObstacleKind::LanePushLeft;
        } else {
            resolved_kind = (std::uniform_int_distribution<int>(0, 1)(rng.engine) == 0)
                ? ObstacleKind::LanePushLeft
                : ObstacleKind::LanePushRight;
        }
        x_pos = constants::LANE_X[lane];
    } else if (kind == ObstacleKind::LanePushLeft || kind == ObstacleKind::LanePushRight) {
        // Shouldn't reach here from random spawner (converted from LaneBlock above),
        // but handle gracefully.
        x_pos = constants::LANE_X[lane];
    }

    Shape   shape        = Shape::Circle;
    uint8_t mask         = 0;
    auto    req_lane_val = int8_t(lane);

    if (kind == ObstacleKind::ShapeGate ||
        kind == ObstacleKind::ComboGate ||
        kind == ObstacleKind::SplitPath) {
        shape = static_cast<Shape>(std::uniform_int_distribution<int>(0, 2)(rng.engine));
    }
    if (kind == ObstacleKind::ComboGate) {
        mask = uint8_t(uint8_t(1 << lane) | uint8_t(1 << ((lane + 1) % 3)));
    }

    apply_obstacle_archetype(reg, obstacle, {
        resolved_kind,
        x_pos,
        constants::SPAWN_Y,
        shape,
        mask,
        req_lane_val
    });

    // Spawn visual mesh children for multi-slab obstacle types
    spawn_obstacle_meshes(reg, obstacle);
}
