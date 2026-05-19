#pragma once

#include <cstdint>
#include <entt/entt.hpp>

#include "player.h"
#include "tags/tags.h"

struct Obstacle {
    int16_t      base_points = 200;

    constexpr Obstacle() = default;
    constexpr explicit Obstacle(int16_t points) : base_points(points) {}
};

// `RequiredShape` (formerly a single-column `{ Shape shape; }` row) was
// eradicated in issue #1202/#1204; the obstacle's required shape lives as
// one of `RequiredShapeCircleTag` / `RequiredShapeSquareTag` /
// `RequiredShapeTriangleTag` / `RequiredShapeHexagonTag` on the obstacle
// entity. Use `set_required_shape_tag()` from `app/util/shape_tag.h` to
// emplace it, and `current_required_shape()` / `has_required_shape_tag()`
// to read it back.

// The former `ShapeGateLane { int8_t lane }` and `RequiredLane { int8_t lane }`
// single-field wrappers were deleted per issue #1198 (companion to the
// `BlockedLanes` → `uint8_t` unwrap in PR #1240 and the `MotionVelocity`
// → `Vector2` unwrap in PR #1241). The per-entity lane index is now stored
// as a raw `int8_t` component.
//
// Slot reservation: the per-entity `int8_t` component is the *lane index*
// for an obstacle entity. Its semantics is determined by the obstacle's
// kind tag (per `app/tags/tags.h`):
//   - `ShapeGateTag`  → positional lane (where the shape hole is)
//   - `SplitPathTag`  → required dodge lane (where the player must be)
// ShapeGate and SplitPath archetypes never coexist on the same entity, so
// the shared `int8_t` slot has no collision. Onset markers and any other
// future obstacle archetype must not emplace a raw `int8_t` on the same
// entity — give them their own per-archetype component table instead.

// Sizing constant for obstacle mesh-child fan-out. Per Fabian Principle 3
// (issue #1554), the inline `children[MAX] + count` array column was
// normalized away: each child entity carries its own `MeshChild { parent }`
// row, and "the children of obstacle X" is now `view<MeshChild>` filtered by
// `parent == X`. The `MAX` cap survives as a hard limit enforced by the
// obstacle mesh factory (`require_child_capacity` in
// app/entities/obstacle_render_entity.cpp).
struct ObstacleChildren {
    static constexpr int MAX = 8;
};
