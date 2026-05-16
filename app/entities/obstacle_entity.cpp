#include "obstacle_entity.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "obstacle_render_entity.h"
#include "../constants.h"
#include "../util/lane_utils.h"
#include "../util/shape_lane_mapping.h"
#include "../util/shape_tag.h"
#include <stdexcept>

namespace {

void finish_obstacle(entt::registry& reg, entt::entity e) {
    spawn_obstacle_meshes(reg, e);
    reg.emplace<ObstacleTag>(e);
}

entt::entity create_shape_gate(entt::registry& reg, const ShapeGateSpawn& params) {
    if (!is_valid_shape(params.shape)) {
        throw std::logic_error("Invalid obstacle shape");
    }
    auto e = reg.create();
    reg.emplace<WorldPosition>(e, WorldPosition{{params.x, params.y}});
    reg.emplace<TagWorldPass>(e);
    reg.emplace<ShapeGateTag>(e);
    reg.emplace<Obstacle>(e, int16_t{constants::PTS_SHAPE_GATE});
    set_required_shape_tag(reg, e, params.shape);
    reg.emplace<int8_t>(
        e, static_cast<int8_t>(lane_utils::nearest_lane_for_x(params.x)));
    reg.emplace<DrawSize>(e, constants::SCREEN_W_F, 80.0f);
    reg.emplace<Color>(e, constants::SHAPE_COLORS[shape_index(params.shape)]);
    return e;
}

entt::entity create_split_path(entt::registry& reg, const SplitPathSpawn& params) {
    if (!is_valid_shape(params.shape)) {
        throw std::logic_error("Invalid obstacle shape");
    }
    if (!lane_utils::is_valid(params.req_lane)) {
        throw std::logic_error("Invalid obstacle lane");
    }
    auto e = reg.create();
    reg.emplace<WorldPosition>(e, WorldPosition{{params.x, params.y}});
    reg.emplace<TagWorldPass>(e);
    reg.emplace<SplitPathTag>(e);
    reg.emplace<Obstacle>(e, int16_t{constants::PTS_SPLIT_PATH});
    set_required_shape_tag(reg, e, params.shape);
    reg.emplace<int8_t>(e, params.req_lane);
    reg.emplace<DrawSize>(e, constants::SCREEN_W_F, 80.0f);
    reg.emplace<Color>(e, Color{255, 215, 0, 255});
    return e;
}

entt::entity create_onset_marker(entt::registry& reg, const OnsetMarkerSpawn& params) {
    auto e = reg.create();
    reg.emplace<WorldPosition>(e, WorldPosition{{params.x, params.y}});
    reg.emplace<TagWorldPass>(e);
    reg.emplace<OnsetMarkerTag>(e);
    reg.emplace<Obstacle>(e, int16_t{0});
    reg.emplace<NonScorableTag>(e);
    reg.emplace<DrawSize>(e, constants::SCREEN_W_F, 80.0f);
    reg.emplace<Color>(e, Color{255, 255, 255, 80});
    return e;
}

} // namespace

entt::entity spawn_shape_gate_obstacle(entt::registry& reg, const ShapeGateSpawn& params) {
    auto e = create_shape_gate(reg, params);
    reg.emplace<Vector2>(e, Vector2{0.0f, params.speed});
    finish_obstacle(reg, e);
    return e;
}

entt::entity spawn_split_path_obstacle(entt::registry& reg, const SplitPathSpawn& params) {
    auto e = create_split_path(reg, params);
    reg.emplace<Vector2>(e, Vector2{0.0f, params.speed});
    finish_obstacle(reg, e);
    return e;
}

entt::entity spawn_onset_marker_obstacle(entt::registry& reg, const OnsetMarkerSpawn& params) {
    auto e = create_onset_marker(reg, params);
    reg.emplace<Vector2>(e, Vector2{0.0f, params.speed});
    finish_obstacle(reg, e);
    return e;
}

entt::entity spawn_shape_gate_rhythm(entt::registry& reg, const ShapeGateSpawn& params,
                                     const BeatInfo& beat_info) {
    auto e = create_shape_gate(reg, params);
    reg.emplace<BeatInfo>(e, beat_info);
    finish_obstacle(reg, e);
    return e;
}

entt::entity spawn_split_path_rhythm(entt::registry& reg, const SplitPathSpawn& params,
                                     const BeatInfo& beat_info) {
    auto e = create_split_path(reg, params);
    reg.emplace<BeatInfo>(e, beat_info);
    finish_obstacle(reg, e);
    return e;
}

entt::entity spawn_onset_marker_rhythm(entt::registry& reg, const OnsetMarkerSpawn& params,
                                       const BeatInfo& beat_info) {
    auto e = create_onset_marker(reg, params);
    reg.emplace<BeatInfo>(e, beat_info);
    finish_obstacle(reg, e);
    return e;
}
