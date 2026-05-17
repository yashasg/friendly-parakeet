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

// Shared finalizers for the six per-kind `spawn_*` wrappers below
// (#1453). All six wrappers were byte-identical apart from which private
// `create_*` builder they invoked: the non-rhythm trio appended a
// `Vector2{0, params.speed}` velocity, the rhythm trio appended a
// `BeatInfo`. Per the byte-identical fold pattern the squad has already
// merged in #1430 / #1436 / #1440 / #1443 / #1448 / #1416, the public
// per-kind symbols remain (call sites in `app/systems/beat_scheduler_system.cpp`
// and several `tests/*.cpp` files reference them by name) and thin-call
// into one of these two finalizers parameterized on the builder.
template <typename Builder, typename Params>
entt::entity spawn_with_velocity(entt::registry& reg, Builder build, const Params& params) {
    auto e = build(reg, params);
    reg.emplace<Vector2>(e, Vector2{0.0f, params.speed});
    finish_obstacle(reg, e);
    return e;
}

template <typename Builder, typename Params>
entt::entity spawn_with_beat(entt::registry& reg, Builder build, const Params& params,
                             const BeatInfo& beat_info) {
    auto e = build(reg, params);
    reg.emplace<BeatInfo>(e, beat_info);
    finish_obstacle(reg, e);
    return e;
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
    return spawn_with_velocity(reg, create_shape_gate, params);
}

entt::entity spawn_split_path_obstacle(entt::registry& reg, const SplitPathSpawn& params) {
    return spawn_with_velocity(reg, create_split_path, params);
}

entt::entity spawn_onset_marker_obstacle(entt::registry& reg, const OnsetMarkerSpawn& params) {
    return spawn_with_velocity(reg, create_onset_marker, params);
}

entt::entity spawn_shape_gate_rhythm(entt::registry& reg, const ShapeGateSpawn& params,
                                     const BeatInfo& beat_info) {
    return spawn_with_beat(reg, create_shape_gate, params, beat_info);
}

entt::entity spawn_split_path_rhythm(entt::registry& reg, const SplitPathSpawn& params,
                                     const BeatInfo& beat_info) {
    return spawn_with_beat(reg, create_split_path, params, beat_info);
}

entt::entity spawn_onset_marker_rhythm(entt::registry& reg, const OnsetMarkerSpawn& params,
                                       const BeatInfo& beat_info) {
    return spawn_with_beat(reg, create_onset_marker, params, beat_info);
}
