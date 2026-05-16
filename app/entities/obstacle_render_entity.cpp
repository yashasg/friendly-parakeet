#include "obstacle_render_entity.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../constants.h"
#include "../util/shape_lane_mapping.h"
#include "../util/shape_tag.h"
#include <cstdint>
#include <stdexcept>

namespace {
uint8_t checked_shape_mesh_index(Shape shape) {
    const int index = shape_index(shape);
    if (index < 0) throw std::logic_error("Invalid required shape tag");
    return static_cast<uint8_t>(index);
}

int checked_lane_index(int8_t lane) {
    const int lane_index = static_cast<int>(lane);
    if (lane_index < 0 || lane_index >= constants::LANE_COUNT) {
        throw std::logic_error("Invalid RequiredLane lane");
    }
    return lane_index;
}

struct PendingEntity {
    entt::registry& reg;
    entt::entity entity;
    bool active = true;

    PendingEntity(entt::registry& registry, entt::entity created)
        : reg(registry), entity(created) {}

    PendingEntity(const PendingEntity&) = delete;
    PendingEntity& operator=(const PendingEntity&) = delete;

    ~PendingEntity() {
        if (active && reg.valid(entity)) {
            reg.destroy(entity);
        }
    }

    void release() {
        active = false;
    }
};
}

static void append_child(entt::registry& reg, entt::entity parent, entt::entity child) {
    auto& children = reg.get_or_emplace<ObstacleChildren>(parent);
    if (children.count >= ObstacleChildren::MAX) {
        throw std::logic_error("ObstacleChildren capacity exceeded");
    }
    children.children[children.count++] = child;
}

// ObstacleChildren::MAX is the hard cap for mesh children owned by one
// logical obstacle. Factory helpers reserve a slot before reg.create() so an
// overflow cannot leave an unregistered MeshChild entity behind.
static void require_child_capacity(entt::registry& reg, entt::entity parent) {
    auto& children = reg.get_or_emplace<ObstacleChildren>(parent);
    if (children.count >= ObstacleChildren::MAX) {
        throw std::logic_error("ObstacleChildren capacity exceeded");
    }
}

static entt::entity add_slab_child(entt::registry& reg, entt::entity parent,
                                    float x, float w, float d, float h, Color tint) {
    require_child_capacity(reg, parent);
    auto e = reg.create();
    PendingEntity pending{reg, e};
    reg.emplace<MeshChild>(e, MeshChild{parent, x, 0.0f, w, d, h, tint});
    reg.emplace<MeshKindSlab>(e);
    reg.emplace<TagWorldPass>(e);
    append_child(reg, parent, e);
    pending.release();
    return e;
}

static entt::entity add_shape_child(entt::registry& reg, entt::entity parent,
                                     uint8_t mesh_index, float cx, float z_offset,
                                     float size, Color tint) {
    require_child_capacity(reg, parent);
    auto e = reg.create();
    PendingEntity pending{reg, e};
    reg.emplace<MeshChild>(e, MeshChild{parent, cx, z_offset, size, 0.0f, 0.0f, tint});
    reg.emplace<MeshKindShape>(e, MeshKindShape{mesh_index});
    reg.emplace<TagWorldPass>(e);
    append_child(reg, parent, e);
    pending.release();
    return e;
}

void spawn_obstacle_meshes(entt::registry& reg, entt::entity logical) {
    const auto* wt_ptr = reg.try_get<WorldTransform>(logical);
    auto& col = reg.get<Color>(logical);
    auto& dsz = reg.get<DrawSize>(logical);

    // Per-kind transforms (issue #1202/#1204). Each obstacle entity
    // carries exactly one kind tag (ShapeGateTag/SplitPathTag/OnsetMarkerTag);
    // each branch below operates on that tag's filtered view of one row.
    if (reg.all_of<ShapeGateTag>(logical)) {
        if (!wt_ptr) return;
        const auto& wt = *wt_ptr;
        const bool has_req = has_required_shape_tag(reg, logical);
        uint8_t mesh_index = 0;
        if (has_req) {
            mesh_index = checked_shape_mesh_index(current_required_shape(reg, logical));
        }
        // Shape gates now render as shape-only prompts (no side walls/slabs).
        if (has_req)
            add_shape_child(reg, logical, mesh_index, wt.position.x, 0.0f,
                            40, col);
        return;
    }
    if (reg.all_of<SplitPathTag>(logical)) {
        auto* rlane = reg.try_get<RequiredLane>(logical);
        int lane_index = 0;
        if (rlane) {
            lane_index = checked_lane_index(rlane->lane);
        }
        const bool has_req = has_required_shape_tag(reg, logical);
        uint8_t mesh_index = 0;
        if (has_req) {
            mesh_index = checked_shape_mesh_index(current_required_shape(reg, logical));
        }
        for (int i = 0; i < constants::LANE_COUNT; ++i)
            if (!rlane || i != lane_index)
                add_slab_child(reg, logical, constants::LANE_X[i]-120,
                               240.0f, dsz.h, constants::OBSTACLE_3D_HEIGHT, col);
        if (has_req && rlane)
            add_shape_child(reg, logical, mesh_index,
                            constants::LANE_X[lane_index],
                            0.0f, 30, {255, 255, 255, 180});
        return;
    }
    if (reg.all_of<OnsetMarkerTag>(logical)) {
        add_slab_child(reg, logical, 0.0f, dsz.w, dsz.h,
                       constants::OBSTACLE_3D_HEIGHT, col);
        return;
    }
}


void destroy_obstacle_mesh_children(entt::registry& reg, entt::entity parent) {
    auto* oc = reg.try_get<ObstacleChildren>(parent);
    if (!oc) return;
    for (int i = 0; i < oc->count; ++i) {
        if (reg.valid(oc->children[i]))
            reg.destroy(oc->children[i]);
        oc->children[i] = entt::null;
    }
    oc->count = 0;
}

void destroy_obstacle_with_children(entt::registry& reg, entt::entity parent) {
    if (!reg.valid(parent)) return;
    destroy_obstacle_mesh_children(reg, parent);
    reg.destroy(parent);
}
