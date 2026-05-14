#include "obstacle_entity.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "obstacle_render_entity.h"
#include "../constants.h"
#include "../util/lane_utils.h"
#include "../util/shape_lane_mapping.h"
#include <stdexcept>

namespace {

bool obstacle_kind_requires_shape(ObstacleKind kind) {
    return kind == ObstacleKind::ShapeGate
        || kind == ObstacleKind::ComboGate
        || kind == ObstacleKind::SplitPath;
}

entt::entity spawn_obstacle_base(entt::registry& reg, const ObstacleSpawnParams& params) {
    if (!obstacle_kind_is_active_runtime_spawnable(params.kind)) {
        throw std::logic_error("Deprecated obstacle kind is not spawnable at runtime");
    }
    if (obstacle_kind_requires_shape(params.kind) && !is_valid_shape(params.shape)) {
        throw std::logic_error("Invalid obstacle shape");
    }

    auto e = reg.create();
    reg.emplace<WorldTransform>(e, WorldTransform{{params.x, params.y}});
    reg.emplace<DrawLayer>(e, Layer::Game);
    reg.emplace<TagWorldPass>(e);

    switch (params.kind) {
        case ObstacleKind::ShapeGate: {
            reg.emplace<Obstacle>(e, int16_t{constants::PTS_SHAPE_GATE});
            reg.emplace<RequiredShape>(e, params.shape);
            reg.emplace<ShapeGateLane>(
                e, static_cast<int8_t>(lane_utils::nearest_lane_for_x(params.x)));
            reg.emplace<DrawSize>(e, constants::SCREEN_W_F, 80.0f);
            reg.emplace<Color>(e, constants::SHAPE_COLORS[shape_index(params.shape)]);
            break;
        }
        case ObstacleKind::LaneBlock:
        case ObstacleKind::ComboGate:
            throw std::logic_error("Deprecated obstacle kind is not spawnable at runtime");
        case ObstacleKind::SplitPath: {
            reg.emplace<Obstacle>(e, int16_t{constants::PTS_SPLIT_PATH});
            reg.emplace<RequiredShape>(e, params.shape);
            reg.emplace<RequiredLane>(e, params.req_lane);
            reg.emplace<DrawSize>(e, constants::SCREEN_W_F, 80.0f);
            reg.emplace<Color>(e, Color{255, 215, 0, 255});
            break;
        }
        case ObstacleKind::OnsetMarker: {
            reg.emplace<Obstacle>(e, int16_t{0});
            reg.emplace<OnsetMarkerTag>(e);
            reg.emplace<NonScorableTag>(e);
            reg.emplace<DrawSize>(e, constants::SCREEN_W_F, 80.0f);
            reg.emplace<Color>(e, Color{255, 255, 255, 80});
            break;
        }
    }

    return e;
}

void finish_obstacle(entt::registry& reg, entt::entity e) {
    spawn_obstacle_meshes(reg, e);
    reg.emplace<ObstacleTag>(e);
}

} // namespace

entt::entity spawn_obstacle(entt::registry& reg, const ObstacleSpawnParams& params) {
    auto e = spawn_obstacle_base(reg, params);
    reg.emplace<MotionVelocity>(e, MotionVelocity{{0.0f, params.speed}});
    finish_obstacle(reg, e);
    return e;
}

entt::entity spawn_rhythm_obstacle(entt::registry& reg, const ObstacleSpawnParams& params,
                                   const BeatInfo& beat_info) {
    auto e = spawn_obstacle_base(reg, params);
    reg.emplace<BeatInfo>(e, beat_info);
    finish_obstacle(reg, e);

    return e;
}
