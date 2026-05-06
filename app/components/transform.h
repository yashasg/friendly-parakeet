#pragma once

#include <cstdint>
#include <SDL.h>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>

// Authoritative world-space spatial state for moving/rendered world entities.
// New entity contracts should use this instead of adding new position structs.
// Named WorldTransform to avoid colliding with legacy global Transform names.
struct WorldTransform {
    glm::vec2 position = {0.0f, 0.0f};
    float   rotation = 0.0f;
    glm::vec2 scale    = {1.0f, 1.0f};
};

// Scoped motion for entities that truly integrate position += velocity * dt.
// Replaces the deleted Velocity struct (issue #349 migration complete).
struct MotionVelocity {
    glm::vec2 value = {0.0f, 0.0f};
};

// Screen-space UI placement. UI entities should use this instead of
// WorldTransform so they do not enter world movement/collision/camera views.
struct UIPosition {
    glm::vec2 value = {0.0f, 0.0f};
};

enum class MeshType : std::uint8_t {
    Slab,
    Shape,
    Quad,
};

struct ModelTransform {
    glm::mat4 mat{1.0f};
    SDL_Color tint{255, 255, 255, 255};
    std::uint8_t mesh_index = 0;
    MeshType mesh_type = MeshType::Slab;
};

using ScreenPosition = glm::vec2;

struct ScreenTransform {
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float scale = 1.0f;
};

constexpr float CAMERA_CLIP_NEAR_DEFAULT = 0.1f;
constexpr float CAMERA_CLIP_FAR_DEFAULT = 1000.0f;

struct CameraClipPlanes {
    float near_plane = CAMERA_CLIP_NEAR_DEFAULT;
    float far_plane = CAMERA_CLIP_FAR_DEFAULT;
};

inline glm::vec2 screen_to_virtual(const glm::vec2& point, const ScreenTransform& st) {
    const float safe_scale = st.scale == 0.0f ? 1.0f : st.scale;
    return {
        (point.x - st.offset_x) / safe_scale,
        (point.y - st.offset_y) / safe_scale,
    };
}
