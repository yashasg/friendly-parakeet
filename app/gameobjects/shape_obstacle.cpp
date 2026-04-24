#include "shape_obstacle.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../components/shape_mesh.h"
#include "../constants.h"

constexpr float OBSTACLE_HEIGHT = 20.0f;

static entt::entity add_slab_child(entt::registry& reg, entt::entity parent,
                                   float x, float w, float d, float h, Color tint) {
    auto e = reg.create();
    reg.emplace<MeshChild>(e, MeshChild{parent, x, 0.0f, w, d, h, tint, MeshType::Slab, 0});
    return e;
}

static entt::entity add_shape_child(entt::registry& reg, entt::entity parent,
                                    Shape shape, float cx, float z_offset,
                                    float size, Color tint) {
    int idx = static_cast<int>(shape);
    auto e = reg.create();
    reg.emplace<MeshChild>(e, MeshChild{parent, cx, z_offset, size, 0, 0, tint,
                                        MeshType::Shape, idx});
    return e;
}

void spawn_obstacle_meshes(entt::registry& reg, entt::entity logical) {
    auto& obs = reg.get<Obstacle>(logical);
    auto& pos = reg.get<Position>(logical);
    auto& col = reg.get<Color>(logical);
    auto& dsz = reg.get<DrawSize>(logical);

    ShapeObstacleGO go{};

    switch (obs.kind) {
        case ObstacleKind::ShapeGate: {
            go.meshes[go.mesh_count++] =
                add_slab_child(reg, logical, 0, pos.x-50, dsz.h, OBSTACLE_HEIGHT, col);
            go.meshes[go.mesh_count++] =
                add_slab_child(reg, logical, pos.x+50,
                    constants::SCREEN_W-pos.x-50, dsz.h, OBSTACLE_HEIGHT, col);
            auto* req = reg.try_get<RequiredShape>(logical);
            if (req)
                go.meshes[go.mesh_count++] =
                    add_shape_child(reg, logical, req->shape, pos.x, dsz.h/2,
                                    40, {col.r, col.g, col.b, 120});
            break;
        }
        case ObstacleKind::LaneBlock: {
            auto* blocked = reg.try_get<BlockedLanes>(logical);
            if (blocked)
                for (int i = 0; i < 3; ++i)
                    if ((blocked->mask >> i) & 1)
                        go.meshes[go.mesh_count++] =
                            add_slab_child(reg, logical, constants::LANE_X[i]-dsz.w/2,
                                           dsz.w, dsz.h, OBSTACLE_HEIGHT, col);
            break;
        }
        case ObstacleKind::ComboGate: {
            auto* blocked = reg.try_get<BlockedLanes>(logical);
            if (blocked)
                for (int i = 0; i < 3; ++i)
                    if ((blocked->mask >> i) & 1)
                        go.meshes[go.mesh_count++] =
                            add_slab_child(reg, logical, constants::LANE_X[i]-120,
                                           240.0f, dsz.h, OBSTACLE_HEIGHT, col);
            auto* req = reg.try_get<RequiredShape>(logical);
            if (req) {
                int open = 1;
                if (blocked)
                    for (int i = 0; i < 3; ++i)
                        if (!((blocked->mask >> i) & 1)) { open = i; break; }
                go.meshes[go.mesh_count++] =
                    add_shape_child(reg, logical, req->shape, constants::LANE_X[open],
                                    dsz.h/2, 30, {255, 255, 255, 180});
            }
            break;
        }
        case ObstacleKind::SplitPath: {
            auto* rlane = reg.try_get<RequiredLane>(logical);
            for (int i = 0; i < 3; ++i)
                if (!rlane || i != rlane->lane)
                    go.meshes[go.mesh_count++] =
                        add_slab_child(reg, logical, constants::LANE_X[i]-120,
                                       240.0f, dsz.h, OBSTACLE_HEIGHT, col);
            auto* req = reg.try_get<RequiredShape>(logical);
            if (req && rlane)
                go.meshes[go.mesh_count++] =
                    add_shape_child(reg, logical, req->shape,
                                    constants::LANE_X[rlane->lane],
                                    dsz.h/2, 30, {255, 255, 255, 180});
            break;
        }
        default:
            break;
    }

    if (go.mesh_count > 0)
        reg.emplace<ShapeObstacleGO>(logical, go);
}

void destroy_obstacle_meshes(entt::registry& reg, entt::entity logical) {
    auto* go = reg.try_get<ShapeObstacleGO>(logical);
    if (!go) return;
    for (int i = 0; i < go->mesh_count; ++i) {
        if (go->meshes[i] != entt::null && reg.valid(go->meshes[i]))
            reg.destroy(go->meshes[i]);
    }
}
