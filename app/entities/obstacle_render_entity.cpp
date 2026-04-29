#include "obstacle_render_entity.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/rendering.h"
#include "../constants.h"
#include <raymath.h>
#include <stdexcept>

namespace {
struct ObstacleModelLifecycleState {
    bool wired = false;
};

bool obstacle_model_lifecycle_wired(entt::registry& reg) {
    auto* state = reg.ctx().find<ObstacleModelLifecycleState>();
    return state && state->wired;
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
    reg.emplace<MeshChild>(e, MeshChild{parent, x, 0.0f, w, d, h, tint, MeshType::Slab, 0});
    reg.emplace<TagWorldPass>(e);
    append_child(reg, parent, e);
    pending.release();
    return e;
}

static entt::entity add_shape_child(entt::registry& reg, entt::entity parent,
                                     Shape shape, float cx, float z_offset,
                                     float size, Color tint) {
    require_child_capacity(reg, parent);
    int idx = static_cast<int>(shape);
    auto e = reg.create();
    PendingEntity pending{reg, e};
    reg.emplace<MeshChild>(e, MeshChild{parent, cx, z_offset, size, 0, 0, tint,
                                        MeshType::Shape, idx});
    reg.emplace<TagWorldPass>(e);
    append_child(reg, parent, e);
    pending.release();
    return e;
}

static bool bar_height_for(ObstacleKind kind, float& height) {
    switch (kind) {
        case ObstacleKind::LowBar:
            height = constants::LOWBAR_3D_HEIGHT;
            return true;
        case ObstacleKind::HighBar:
            height = constants::HIGHBAR_3D_HEIGHT;
            return true;
        default:
            return false;
    }
}

void spawn_obstacle_meshes(entt::registry& reg, entt::entity logical) {
    auto& obs = reg.get<Obstacle>(logical);
    const auto* pos_ptr = reg.try_get<Position>(logical);
    auto& col = reg.get<Color>(logical);
    auto& dsz = reg.get<DrawSize>(logical);

    switch (obs.kind) {
        case ObstacleKind::ShapeGate: {
            if (!pos_ptr) break;
            const auto& pos = *pos_ptr;
            add_slab_child(reg, logical, 0, pos.x-50, dsz.h,
                           constants::OBSTACLE_3D_HEIGHT, col);
            add_slab_child(reg, logical, pos.x+50,
                constants::SCREEN_W-pos.x-50, dsz.h,
                constants::OBSTACLE_3D_HEIGHT, col);
            auto* req = reg.try_get<RequiredShape>(logical);
            if (req)
                add_shape_child(reg, logical, req->shape, pos.x, dsz.h/2,
                                40, {col.r, col.g, col.b, 120});
            break;
        }
        case ObstacleKind::LaneBlock: {
            auto* blocked = reg.try_get<BlockedLanes>(logical);
            if (blocked)
                for (int i = 0; i < 3; ++i)
                    if ((blocked->mask >> i) & 1)
                        add_slab_child(reg, logical, constants::LANE_X[i]-dsz.w/2,
                                       dsz.w, dsz.h, constants::OBSTACLE_3D_HEIGHT, col);
            break;
        }
        case ObstacleKind::ComboGate: {
            auto* blocked = reg.try_get<BlockedLanes>(logical);
            if (blocked)
                for (int i = 0; i < 3; ++i)
                    if ((blocked->mask >> i) & 1)
                        add_slab_child(reg, logical, constants::LANE_X[i]-120,
                                       240.0f, dsz.h, constants::OBSTACLE_3D_HEIGHT, col);
            auto* req = reg.try_get<RequiredShape>(logical);
            if (req) {
                int open = 1;
                if (blocked)
                    for (int i = 0; i < 3; ++i)
                        if (!((blocked->mask >> i) & 1)) { open = i; break; }
                add_shape_child(reg, logical, req->shape, constants::LANE_X[open],
                                dsz.h/2, 30, {255, 255, 255, 180});
            }
            break;
        }
        case ObstacleKind::SplitPath: {
            auto* rlane = reg.try_get<RequiredLane>(logical);
            for (int i = 0; i < 3; ++i)
                if (!rlane || i != rlane->lane)
                    add_slab_child(reg, logical, constants::LANE_X[i]-120,
                                   240.0f, dsz.h, constants::OBSTACLE_3D_HEIGHT, col);
            auto* req = reg.try_get<RequiredShape>(logical);
            if (req && rlane)
                add_shape_child(reg, logical, req->shape,
                                constants::LANE_X[rlane->lane],
                                dsz.h/2, 30, {255, 255, 255, 180});
            break;
        }
        default:
            break;
    }
}

void build_obstacle_model(entt::registry& reg, entt::entity logical) {
    if (!IsWindowReady()) return;

    auto* obs = reg.try_get<Obstacle>(logical);
    auto* dsz = reg.try_get<DrawSize>(logical);
    if (!obs || !dsz) return;

    float height = 0.0f;
    if (!bar_height_for(obs->kind, height)) return;
    if (!obstacle_model_lifecycle_wired(reg)) {
        throw std::logic_error("build_obstacle_model requires wire_obstacle_model_lifecycle() first");
    }

    // Manual model construction — never use LoadModelFromMesh (opaque + GPU-implicit).
    // raylib 5.5: GenMeshCube already returns an uploaded mesh; no UploadMesh needed.
    Model model = {};
    model.transform     = MatrixIdentity();
    model.meshCount     = 1;
    model.meshes        = static_cast<Mesh*>(RL_MALLOC(sizeof(Mesh)));
    model.meshes[0]     = GenMeshCube(1.0f, 1.0f, 1.0f);  // unit cube; slab_matrix scales to world dims
    model.materialCount = 1;
    model.materials     = static_cast<Material*>(RL_MALLOC(sizeof(Material)));
    model.materials[0]  = LoadMaterialDefault();
    model.meshMaterial  = static_cast<int*>(RL_CALLOC(model.meshCount, sizeof(int)));
    // meshMaterial[0] == 0 via RL_CALLOC

    if (reg.all_of<ObstacleModel>(logical)) {
        on_obstacle_model_destroy(reg, logical);
        reg.remove<ObstacleModel>(logical);
    }
    auto& om = reg.emplace<ObstacleModel>(logical);
    om.model = model;
    om.owned = true;

    auto& pd    = reg.get_or_emplace<ObstacleParts>(logical);
    pd.cx       = 0.0f;
    pd.cy       = 0.0f;
    pd.cz       = 0.0f;
    pd.width    = constants::SCREEN_W_F;
    pd.height   = height;
    pd.depth    = dsz->h;

    reg.get_or_emplace<TagWorldPass>(logical);
}

void on_obstacle_model_destroy(entt::registry& reg, entt::entity entity) {
    auto* om = reg.try_get<ObstacleModel>(entity);
    if (!om || !om->owned) return;

    if (om->model.meshes) {
        UnloadModel(om->model);
    }
    om->model = Model{};
    om->owned = false;
}

void wire_obstacle_model_lifecycle(entt::registry& reg) {
    auto* state = reg.ctx().find<ObstacleModelLifecycleState>();
    if (!state) {
        state = &reg.ctx().emplace<ObstacleModelLifecycleState>();
    }
    if (state->wired) return;

    reg.on_destroy<ObstacleModel>().connect<&on_obstacle_model_destroy>();
    state->wired = true;
}

void unwire_obstacle_model_lifecycle(entt::registry& reg) {
    reg.on_destroy<ObstacleModel>().disconnect<&on_obstacle_model_destroy>();
    if (auto* state = reg.ctx().find<ObstacleModelLifecycleState>()) {
        state->wired = false;
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
