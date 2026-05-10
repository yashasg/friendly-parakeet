#pragma once

#include <glm/vec2.hpp>

// Authoritative world-space spatial state for moving/rendered world entities.
// New entity contracts should use this instead of adding new position structs.
// Named WorldTransform to avoid colliding with raylib's global Transform type.
struct WorldTransform {
    glm::vec2 position = {0.0f, 0.0f};
    float     rotation = 0.0f;
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
