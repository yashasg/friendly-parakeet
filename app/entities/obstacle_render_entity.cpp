#include "obstacle_render_entity.h"
#include "../components/obstacle.h"
#include "../components/transform.h"
#include "../components/render_tags.h"
#include "../components/registry_context.h"
#include "../constants.h"
#include "../systems/platform_state.h"
#include "../systems/render_api.h"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {
struct ObstacleLifecycleWiringState {
    bool mesh_connected = false;
    bool model_connected = false;
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

void on_obstacle_destroy(entt::registry& reg, entt::entity parent) {
    auto* oc = reg.try_get<ObstacleChildren>(parent);
    if (!oc) return;
    for (int i = 0; i < oc->count; ++i) {
        if (reg.valid(oc->children[i])) {
            reg.destroy(oc->children[i]);
        }
    }
}

void on_obstacle_model_destroy(entt::registry& reg, entt::entity entity) {
    auto* om = reg.try_get<ObstacleModel>(entity);
    if (!om || !om->owned) return;

    if (om->model.meshes) {
        render_api::unload_model(om->model);
    }
    om->model = Model{};
    om->owned = false;
}
}  // namespace

void wire_obstacle_mesh_lifetime(entt::registry& reg) {
    auto& wiring = registry_ctx_get_or_emplace<ObstacleLifecycleWiringState>(reg);
    if (wiring.mesh_connected) return;
    reg.storage<ObstacleChildren>();
    reg.on_destroy<ObstacleChildren>().connect<&on_obstacle_destroy>();
    wiring.mesh_connected = true;
}

void unwire_obstacle_mesh_lifetime(entt::registry& reg) {
    auto* wiring = registry_ctx_find<ObstacleLifecycleWiringState>(reg);
    if (!wiring || !wiring->mesh_connected) return;
    reg.on_destroy<ObstacleChildren>().disconnect<&on_obstacle_destroy>();
    wiring->mesh_connected = false;
}

static void append_child(entt::registry& reg, entt::entity parent, entt::entity child) {
    auto& children = reg.get_or_emplace<ObstacleChildren>(parent);
    if (children.count >= ObstacleChildren::MAX) {
        throw std::logic_error("ObstacleChildren capacity exceeded");
    }
    children.children[children.count++] = child;
}

static void add_slab_child(entt::registry& reg, entt::entity parent,
                           float x, float w, float d, float h, SDL_Color tint) {
    auto e = reg.create();
    PendingEntity pending{reg, e};
    reg.emplace<MeshChild>(e, MeshChild{parent, x, 0.0f, w, d, h, tint, 0, MeshType::Slab});
    reg.emplace<TagWorldPass>(e);
    append_child(reg, parent, e);
    pending.release();
}

static void add_shape_child(entt::registry& reg, entt::entity parent,
                            uint8_t mesh_index, float cx, float z_offset,
                            float size, SDL_Color tint) {
    auto e = reg.create();
    PendingEntity pending{reg, e};
    reg.emplace<MeshChild>(e, MeshChild{parent, cx, z_offset, size, 0.0f, 0.0f, tint,
                                        mesh_index, MeshType::Shape});
    reg.emplace<TagWorldPass>(e);
    append_child(reg, parent, e);
    pending.release();
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
    wire_obstacle_mesh_lifetime(reg);

    auto& obs = reg.get<Obstacle>(logical);
    const auto* wt_ptr = reg.try_get<WorldTransform>(logical);
    auto& col = reg.get<SDL_Color>(logical);
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

void build_obstacle_model(entt::registry& reg, entt::entity logical) {
    if (!platform_state::is_ready()) return;

    auto* obs = reg.try_get<Obstacle>(logical);
    auto* dsz = reg.try_get<DrawSize>(logical);
    if (!obs || !dsz) return;

    float height = 0.0f;
    if (!bar_height_for(obs->kind, height)) return;
    wire_obstacle_model_lifecycle(reg);

    // Manual model construction — never use LoadModelFromMesh (opaque + GPU-implicit).
    // Runtime mesh generator returns upload-ready mesh data; no explicit UploadMesh step.
    Model model = {};
    model.transform     = glm::mat4{1.0f};
    model.meshCount     = 1;
    model.meshes        = static_cast<Mesh*>(std::malloc(sizeof(Mesh)));
    model.meshes[0]     = render_api::gen_mesh_cube(1.0f, 1.0f, 1.0f);  // unit cube; slab_matrix scales to world dims
    model.materialCount = 1;
    model.materials     = static_cast<Material*>(std::malloc(sizeof(Material)));
    model.materials[0]  = render_api::load_material_default();
    model.meshMaterial  = static_cast<int*>(std::calloc(static_cast<std::size_t>(model.meshCount), sizeof(int)));
    // meshMaterial[0] == 0 via calloc

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

void wire_obstacle_model_lifecycle(entt::registry& reg) {
    auto& wiring = registry_ctx_get_or_emplace<ObstacleLifecycleWiringState>(reg);
    if (wiring.model_connected) return;
    reg.on_destroy<ObstacleModel>().connect<&on_obstacle_model_destroy>();
    wiring.model_connected = true;
}

void unwire_obstacle_model_lifecycle(entt::registry& reg) {
    auto* wiring = registry_ctx_find<ObstacleLifecycleWiringState>(reg);
    if (!wiring || !wiring->model_connected) return;
    reg.on_destroy<ObstacleModel>().disconnect<&on_obstacle_model_destroy>();
    wiring->model_connected = false;
}
