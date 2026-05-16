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

// Owned mesh children for a logical obstacle entity. destroy_obstacle_with_children
// in app/entities/obstacle_render_entity.cpp cleans up in O(count). Relocated out
// of app/components/rendering.h (issue #1194 SPLIT) — child-ownership is an
// obstacle-archetype concern, not a generic render concept.
struct ObstacleChildren {
    static constexpr int MAX = 8;
    entt::entity children[MAX]{};
    int count = 0;
};
