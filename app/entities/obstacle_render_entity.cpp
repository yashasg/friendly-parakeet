#include "obstacle_render_entity.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../constants.h"
#include <cstdint>
#include <stdexcept>

namespace {
struct ObstacleMeshLifetimeState {
    entt::registry* owner = nullptr;
    entt::scoped_connection connection;
};


uint8_t checked_shape_mesh_index(Shape shape) {
    switch (shape) {
        case Shape::Circle:
            return 0;
        case Shape::Square:
            return 1;
        case Shape::Triangle:
            return 2;
        case Shape::Hexagon:
            return 3;
    }

    throw std::logic_error("Invalid RequiredShape shape");
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

void on_obstacle_destroy(entt::registry& reg, entt::entity parent);

void wire_obstacle_mesh_lifetime(entt::registry& reg) {
    auto* state = reg.ctx().find<ObstacleMeshLifetimeState>();
    if (!state) {
        state = &reg.ctx().emplace<ObstacleMeshLifetimeState>();
    }
    if (state->owner == &reg) return;

    reg.storage<ObstacleChildren>();
    state->connection = entt::scoped_connection{
        reg.on_destroy<ObstacleChildren>().connect<&on_obstacle_destroy>()};
    state->owner = &reg;
}

void unwire_obstacle_mesh_lifetime(entt::registry& reg) {
    if (auto* state = reg.ctx().find<ObstacleMeshLifetimeState>()) {
        *state = ObstacleMeshLifetimeState{};
    }
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
    reg.emplace<MeshChild>(e, MeshChild{parent, x, 0.0f, w, d, h, tint, 0, MeshType::Slab});
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
    reg.emplace<MeshChild>(e, MeshChild{parent, cx, z_offset, size, 0.0f, 0.0f, tint,
                                        mesh_index, MeshType::Shape});
    reg.emplace<TagWorldPass>(e);
    append_child(reg, parent, e);
    pending.release();
    return e;
}

void spawn_obstacle_meshes(entt::registry& reg, entt::entity logical) {
    wire_obstacle_mesh_lifetime(reg);

    auto& obs = reg.get<Obstacle>(logical);
    const auto* wt_ptr = reg.try_get<WorldTransform>(logical);
    auto& col = reg.get<Color>(logical);
    auto& dsz = reg.get<DrawSize>(logical);

    switch (obs.kind) {
        case ObstacleKind::ShapeGate: {
            if (!wt_ptr) break;
            const auto& wt = *wt_ptr;
            auto* req = reg.try_get<RequiredShape>(logical);
            uint8_t mesh_index = 0;
            if (req) {
                mesh_index = checked_shape_mesh_index(req->shape);
            }
            // Shape gates now render as shape-only prompts (no side walls/slabs).
            if (req)
                add_shape_child(reg, logical, mesh_index, wt.position.x, 0.0f,
                                40, col);
            break;
        }
        case ObstacleKind::LaneBlock: {
            auto* blocked = reg.try_get<BlockedLanes>(logical);
            if (blocked)
                for (int i = 0; i < constants::LANE_COUNT; ++i)
                    if ((blocked->mask >> i) & 1)
                        add_slab_child(reg, logical, constants::LANE_X[i]-dsz.w/2,
                                       dsz.w, dsz.h, constants::OBSTACLE_3D_HEIGHT, col);
            break;
        }
        case ObstacleKind::ComboGate: {
            auto* blocked = reg.try_get<BlockedLanes>(logical);
            auto* req = reg.try_get<RequiredShape>(logical);
            uint8_t mesh_index = 0;
            if (req) {
                mesh_index = checked_shape_mesh_index(req->shape);
            }
            if (blocked)
                for (int i = 0; i < constants::LANE_COUNT; ++i)
                    if ((blocked->mask >> i) & 1)
                        add_slab_child(reg, logical, constants::LANE_X[i]-120,
                                       240.0f, dsz.h, constants::OBSTACLE_3D_HEIGHT, col);
            if (req) {
                int open = 1;
                if (blocked)
                    for (int i = 0; i < constants::LANE_COUNT; ++i)
                        if (!((blocked->mask >> i) & 1)) { open = i; break; }
                add_shape_child(reg, logical, mesh_index, constants::LANE_X[open],
                                 0.0f, 30, {255, 255, 255, 180});
            }
            break;
        }
        case ObstacleKind::SplitPath: {
            auto* rlane = reg.try_get<RequiredLane>(logical);
            int lane_index = 0;
            if (rlane) {
                lane_index = checked_lane_index(rlane->lane);
            }
            auto* req = reg.try_get<RequiredShape>(logical);
            uint8_t mesh_index = 0;
            if (req) {
                mesh_index = checked_shape_mesh_index(req->shape);
            }
            for (int i = 0; i < constants::LANE_COUNT; ++i)
                if (!rlane || i != lane_index)
                    add_slab_child(reg, logical, constants::LANE_X[i]-120,
                                   240.0f, dsz.h, constants::OBSTACLE_3D_HEIGHT, col);
            if (req && rlane)
                add_shape_child(reg, logical, mesh_index,
                                constants::LANE_X[lane_index],
                                0.0f, 30, {255, 255, 255, 180});
            break;
        }
        default:
            break;
    }
}


// on_destroy listener: destroy MeshChild entities owned by this parent.
// Uses ObstacleChildren for O(N) lookup instead of scanning the full pool.
void on_obstacle_destroy(entt::registry& reg, entt::entity parent) {
    auto* oc = reg.try_get<ObstacleChildren>(parent);
    if (!oc) return;
    for (int i = 0; i < oc->count; ++i) {
        if (reg.valid(oc->children[i]))
            reg.destroy(oc->children[i]);
    }
}
