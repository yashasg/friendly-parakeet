#pragma once
#include <entt/entt.hpp>
#include <glm/vec3.hpp>
#include <stdexcept>
#include <type_traits>
#include "../util/render_types.h"  // Mesh, free_mesh_data

enum CameraProjectionMode : int {
    CAMERA_PERSPECTIVE = 0,
    CAMERA_ORTHOGRAPHIC = 1,
};

struct Camera3D {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float fovy = 45.0f;
    int projection = CAMERA_PERSPECTIVE;
};

// Per-entity camera data component.
struct GameCamera { Camera3D cam; };

// Create the singleton game camera entity (3D perspective, fixed position).
// Throws std::logic_error if the registry already has one.
void spawn_game_camera(entt::registry& reg);

// Ensure the singleton camera entity exists. Safe to call repeatedly.
void ensure_game_camera(entt::registry& reg);

namespace detail {

template <typename Registry>
entt::entity try_game_camera_entity_impl(Registry& reg) {
    using CameraComponent =
        std::conditional_t<std::is_const_v<Registry>, const GameCamera, GameCamera>;
    auto view = reg.template view<CameraComponent>();
    auto it = view.begin();
    if (it == view.end()) {
        return entt::null;
    }
    const auto entity = *it;
    if (++it != view.end()) {
        throw std::logic_error("multiple GameCamera entities exist");
    }
    return entity;
}

template <typename Registry>
decltype(auto) try_game_camera_impl(Registry& reg) {
    using CameraComponent =
        std::conditional_t<std::is_const_v<Registry>, const GameCamera, GameCamera>;
    const entt::entity entity = try_game_camera_entity_impl(reg);
    if (entity == entt::null) {
        return static_cast<CameraComponent*>(nullptr);
    }
    return reg.template try_get<CameraComponent>(entity);
}

template <typename Registry>
decltype(auto) game_camera_impl(Registry& reg) {
    if (auto* camera = try_game_camera_impl(reg)) {
        return *camera;
    }
    throw std::logic_error("GameCamera entity is missing; call spawn_game_camera() first");
}

}  // namespace detail

inline entt::entity try_game_camera_entity(entt::registry& reg) {
    return detail::try_game_camera_entity_impl(reg);
}

inline entt::entity try_game_camera_entity(const entt::registry& reg) {
    return detail::try_game_camera_entity_impl(reg);
}

inline GameCamera* try_game_camera(entt::registry& reg) {
    return detail::try_game_camera_impl(reg);
}

inline const GameCamera* try_game_camera(const entt::registry& reg) {
    return detail::try_game_camera_impl(reg);
}

// Accessors — safe after camera entity creation.
inline GameCamera& game_camera(entt::registry& reg) {
    return detail::game_camera_impl(reg);
}

inline const GameCamera& game_camera(const entt::registry& reg) {
    return detail::game_camera_impl(reg);
}

namespace camera {

// ── ShapeMeshes ───────────────────────────────────────────────────────────────
// RAII holder for CPU-side shape geometry used by the software-3D renderer.
// Non-copyable (each Mesh owns heap memory); move transfers ownership.
// owned == true means this instance must free mesh vertex data on destruction.
struct ShapeMeshes {
    static constexpr int SHAPE_COUNT = 4;

    Mesh shapes[SHAPE_COUNT] = {};  // circle(12), square, triangle(3), hexagon(6)
    Mesh slab                = {};
    Mesh quad                = {};
    bool owned               = false;

    ShapeMeshes() = default;

    ~ShapeMeshes() { release(); }

    ShapeMeshes(const ShapeMeshes&)            = delete;
    ShapeMeshes& operator=(const ShapeMeshes&) = delete;

    ShapeMeshes(ShapeMeshes&& o) noexcept { *this = std::move(o); }

    ShapeMeshes& operator=(ShapeMeshes&& o) noexcept {
        if (this != &o) {
            release();
            for (int i = 0; i < SHAPE_COUNT; ++i) {
                shapes[i] = o.shapes[i];
                o.shapes[i] = Mesh{};
            }
            slab    = o.slab;  o.slab = Mesh{};
            quad    = o.quad;  o.quad = Mesh{};
            owned   = o.owned; o.owned = false;
        }
        return *this;
    }

    void release() noexcept {
        if (!owned) return;
        for (auto& m : shapes) free_mesh_data(m);
        free_mesh_data(slab);
        free_mesh_data(quad);
        owned = false;
    }
};

}  // namespace camera
