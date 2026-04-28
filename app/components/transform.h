#pragma once

#include <raylib.h>

// Authoritative world-space spatial state for moving/rendered world entities.
// New entity contracts should use this instead of adding new position structs.
// Named WorldTransform to avoid colliding with raylib's global Transform type.
struct WorldTransform {
    Vector2 position = {0.0f, 0.0f};
    float   rotation = 0.0f;
    Vector2 scale    = {1.0f, 1.0f};
};

// Scoped motion for entities that truly integrate position += velocity * dt.
// This replaces generic Velocity during the issue #349 migration.
struct MotionVelocity {
    Vector2 value = {0.0f, 0.0f};
};

// Screen-space UI placement. UI entities should use this instead of
// WorldTransform so they do not enter world movement/collision/camera views.
struct UIPosition {
    Vector2 value = {0.0f, 0.0f};
};

// Legacy spatial data kept temporarily while systems migrate to WorldTransform,
// MotionVelocity, and UIPosition.
struct Position {
    float x = 0.0f;
    float y = 0.0f;
};

// Legacy generic motion kept temporarily while systems migrate to MotionVelocity.
struct Velocity {
    float dx = 0.0f;
    float dy = 0.0f;
};
