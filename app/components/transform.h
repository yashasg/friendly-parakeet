#pragma once

#include <raylib.h>

// Authoritative world-space spatial state for moving/rendered world entities.
// New entity contracts should use this instead of adding new position structs.
// Named WorldTransform to avoid colliding with raylib's global Transform type.
//
// Rotation and scale fields previously lived here but no runtime system read
// them (only defaults were asserted in tests). Removed per issue #1194; if a
// future entity needs rotation or scale, give it its own component table.
struct WorldTransform {
    Vector2 position = {0.0f, 0.0f};
};

// Velocity vector (units per second) for entities that integrate
// position += velocity * dt. Raw Vector2 is used directly — the
// MotionVelocity { Vector2 value } single-field wrapper was deleted
// per issue #1198 (companion to the BlockedLanes → uint8_t unwrap in
// PR #1240). motion_system queries view<WorldTransform, Vector2>;
// existence of a Vector2 component on an entity is the ownership
// marker for dt-integrated movement. Song-time-authoritative rhythm
// obstacles intentionally omit Vector2 and carry BeatInfo instead.
//
// Slot reservation: the per-entity Vector2 component slot is reserved
// for "velocity" semantics. The other Vector2-shaped wrappers in
// issue #1198 (ScreenPosition {float x, y}, DrawSize {float w, h})
// must remain as named structs to avoid colliding on popup/obstacle
// archetypes that also carry a velocity.
