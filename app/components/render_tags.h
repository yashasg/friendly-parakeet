#pragma once

#include <cstdint>
#include <entt/entt.hpp>
#include <SDL.h>
#include <utility>
#include "transform.h"              // MeshType
#include "../util/render_types.h"   // Model, Mesh, Material

// ── Render-pass membership tags ──────────────────────────────────────────────
// Empty tag components that declare which render pass an entity belongs to.
// game_render_system queries each pass view independently.
// One tag per entity.
struct TagWorldPass   {};  // drawn in 3D world geometry pass
struct TagEffectsPass {};  // particles and post-process overlays
struct TagHUDPass     {};  // screen-space UI elements

// ── Draw size / layer metadata ────────────────────────────────────────────────
struct DrawSize {
    float w = 64.0f;
    float h = 64.0f;
};

enum class Layer : uint8_t {
    Background = 0,
    Game       = 1,
    Effects    = 2,
    HUD        = 3,
};

struct DrawLayer {
    Layer layer = Layer::Game;
};

// ── Floor pulse render parameters ────────────────────────────────────────────
// Computed each frame in game_camera_system from SongState beat pulse.
struct FloorParams {
    float   size  = 0.0f;
    float   half  = 0.0f;
    float   thick = 0.0f;
    uint8_t alpha = 0;
};

// ── Geometry descriptor for a Model-authority obstacle ────────────────────────
// Populated at spawn alongside ObstacleModel.
// game_camera_system reads these + ObstacleScrollZ.z to recompute transforms.
struct ObstacleParts {
    float cx     = 0.0f;
    float cy     = 0.0f;
    float cz     = 0.0f;
    float width  = 0.0f;
    float height = 0.0f;
    float depth  = 0.0f;
};

// ── Mesh child of a logical entity ───────────────────────────────────────────
// game_camera_system reads parent WorldTransform + child offsets to compute
// ModelTransform each frame.
struct MeshChild {
    entt::entity parent;
    float     x          = 0.0f;
    float     z_offset   = 0.0f;
    float     width      = 0.0f;
    float     depth      = 0.0f;
    float     height     = 0.0f;
    SDL_Color tint       = {255, 255, 255, 255};
    uint8_t   mesh_index = 0;
    MeshType  mesh_type  = MeshType::Slab;
};

// ── Mesh-child ownership ──────────────────────────────────────────────────────
// Emplaced on logical obstacle entities; on_obstacle_destroy reads this to
// destroy children in O(count) without scanning the full MeshChild pool.
struct ObstacleChildren {
    static constexpr int MAX = 8;
    entt::entity children[MAX] = {};
    int count = 0;
};

// ── Model-authority obstacle render component ─────────────────────────────────
// Carries the CPU-side Model (geometry + transform) for LowBar/HighBar obstacles.
// owned == true means this instance must call render_api::unload_model on destroy.
// Non-copyable: copying a heap-allocated model would alias the mesh pointers.
struct ObstacleModel {
    Model model;
    bool  owned = false;

    ObstacleModel() = default;

    ObstacleModel(const ObstacleModel&)            = delete;
    ObstacleModel& operator=(const ObstacleModel&) = delete;

    ObstacleModel(ObstacleModel&&) noexcept            = default;
    ObstacleModel& operator=(ObstacleModel&&) noexcept = default;
};

struct RenderTargets {
    SDL_Texture* world = nullptr;
    SDL_Texture* ui = nullptr;
    bool owned = false;

    RenderTargets() = default;
    RenderTargets(SDL_Texture* world_target, SDL_Texture* ui_target)
        : world(world_target), ui(ui_target), owned(world_target != nullptr || ui_target != nullptr) {}

    RenderTargets(const RenderTargets&) = delete;
    RenderTargets& operator=(const RenderTargets&) = delete;

    RenderTargets(RenderTargets&& other) noexcept {
        *this = std::move(other);
    }

    RenderTargets& operator=(RenderTargets&& other) noexcept {
        if (this != &other) {
            release();
            world = other.world;
            ui = other.ui;
            owned = other.owned;
            other.world = nullptr;
            other.ui = nullptr;
            other.owned = false;
        }
        return *this;
    }

    ~RenderTargets() { release(); }

    [[nodiscard]] bool ready() const noexcept {
        return world != nullptr && ui != nullptr;
    }

    void release() noexcept {
        if (owned) {
            if (world) SDL_DestroyTexture(world);
            if (ui) SDL_DestroyTexture(ui);
        }
        world = nullptr;
        ui = nullptr;
        owned = false;
    }
};
