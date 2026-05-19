#pragma once

#include "../components/player.h"
#include "../tags/tags.h"
#include "shape_lane_mapping.h"

#include <array>
#include <entt/entt.hpp>

// Non-tag helper for per-shape tag table dispatch (issue #1202/#1204).
// Replaces the former Shape-typed on-entity fields (`PlayerShape::current`,
// `ShapeWindow::target_shape`, `RequiredShape::shape`) with per-shape tag
// presence. Same lookup-table mechanic as `kShapeFlatDrawFns` in PR #1259
// — function-pointer-per-row, indexed by `shape_index(shape)`, no `switch`
// on a discriminator.
//
// `app/tags/tags.h` remains the only tag declaration header. This helper lives
// in `app/util/` because it declares no tags and depends on the non-tag `Shape`
// type from `app/components/player.h`.
//
// Each table has the same shape: four rows, one per former `Shape` value.
// `set_*_shape_tag` writes the table (removes all four tags, emplaces the
// matching one); `current_*_shape` reads the table back (returns the Shape
// value the lone present tag corresponds to, or Shape::Hexagon as the
// no-tag fallback for the legacy "no playable shape" sentinel).

namespace shape_tag_detail {

using EmplaceFn = void (*)(entt::registry&, entt::entity);
using RemoveFn  = void (*)(entt::registry&, entt::entity);
using HasFn     = bool (*)(const entt::registry&, entt::entity);

// ── Player current-shape table ──
inline void emplace_player_circle  (entt::registry& r, entt::entity e) { r.emplace_or_replace<ShapeCircleTag>(e); }
inline void emplace_player_square  (entt::registry& r, entt::entity e) { r.emplace_or_replace<ShapeSquareTag>(e); }
inline void emplace_player_triangle(entt::registry& r, entt::entity e) { r.emplace_or_replace<ShapeTriangleTag>(e); }
inline void emplace_player_hexagon (entt::registry& r, entt::entity e) { r.emplace_or_replace<ShapeHexagonTag>(e); }
inline bool has_player_circle  (const entt::registry& r, entt::entity e) { return r.all_of<ShapeCircleTag>(e); }
inline bool has_player_square  (const entt::registry& r, entt::entity e) { return r.all_of<ShapeSquareTag>(e); }
inline bool has_player_triangle(const entt::registry& r, entt::entity e) { return r.all_of<ShapeTriangleTag>(e); }
inline bool has_player_hexagon (const entt::registry& r, entt::entity e) { return r.all_of<ShapeHexagonTag>(e); }

inline constexpr std::array<EmplaceFn, kShapeCount> kEmplacePlayer{
    &emplace_player_circle,
    &emplace_player_square,
    &emplace_player_triangle,
    &emplace_player_hexagon,
};
inline constexpr std::array<HasFn, kShapeCount> kHasPlayer{
    &has_player_circle,
    &has_player_square,
    &has_player_triangle,
    &has_player_hexagon,
};

// ── Shape window target-shape table ──
inline void emplace_target_circle  (entt::registry& r, entt::entity e) { r.emplace_or_replace<TargetShapeCircleTag>(e); }
inline void emplace_target_square  (entt::registry& r, entt::entity e) { r.emplace_or_replace<TargetShapeSquareTag>(e); }
inline void emplace_target_triangle(entt::registry& r, entt::entity e) { r.emplace_or_replace<TargetShapeTriangleTag>(e); }
inline void emplace_target_hexagon (entt::registry& r, entt::entity e) { r.emplace_or_replace<TargetShapeHexagonTag>(e); }
inline bool has_target_circle  (const entt::registry& r, entt::entity e) { return r.all_of<TargetShapeCircleTag>(e); }
inline bool has_target_square  (const entt::registry& r, entt::entity e) { return r.all_of<TargetShapeSquareTag>(e); }
inline bool has_target_triangle(const entt::registry& r, entt::entity e) { return r.all_of<TargetShapeTriangleTag>(e); }
inline bool has_target_hexagon (const entt::registry& r, entt::entity e) { return r.all_of<TargetShapeHexagonTag>(e); }

inline constexpr std::array<EmplaceFn, kShapeCount> kEmplaceTarget{
    &emplace_target_circle,
    &emplace_target_square,
    &emplace_target_triangle,
    &emplace_target_hexagon,
};
inline constexpr std::array<HasFn, kShapeCount> kHasTarget{
    &has_target_circle,
    &has_target_square,
    &has_target_triangle,
    &has_target_hexagon,
};

// ── Obstacle required-shape table ──
inline void emplace_required_circle  (entt::registry& r, entt::entity e) { r.emplace_or_replace<RequiredShapeCircleTag>(e); }
inline void emplace_required_square  (entt::registry& r, entt::entity e) { r.emplace_or_replace<RequiredShapeSquareTag>(e); }
inline void emplace_required_triangle(entt::registry& r, entt::entity e) { r.emplace_or_replace<RequiredShapeTriangleTag>(e); }
inline void emplace_required_hexagon (entt::registry& r, entt::entity e) { r.emplace_or_replace<RequiredShapeHexagonTag>(e); }
inline bool has_required_circle  (const entt::registry& r, entt::entity e) { return r.all_of<RequiredShapeCircleTag>(e); }
inline bool has_required_square  (const entt::registry& r, entt::entity e) { return r.all_of<RequiredShapeSquareTag>(e); }
inline bool has_required_triangle(const entt::registry& r, entt::entity e) { return r.all_of<RequiredShapeTriangleTag>(e); }
inline bool has_required_hexagon (const entt::registry& r, entt::entity e) { return r.all_of<RequiredShapeHexagonTag>(e); }

inline constexpr std::array<EmplaceFn, kShapeCount> kEmplaceRequired{
    &emplace_required_circle,
    &emplace_required_square,
    &emplace_required_triangle,
    &emplace_required_hexagon,
};
inline constexpr std::array<HasFn, kShapeCount> kHasRequired{
    &has_required_circle,
    &has_required_square,
    &has_required_triangle,
    &has_required_hexagon,
};

}  // namespace shape_tag_detail

// Sets the player entity's current-shape tag. Removes all four `Shape*Tag`
// first, then emplaces the matching one (no-op for invalid Shape values).
// Maintains the "exactly one Shape*Tag per player" invariant.
inline void set_player_shape_tag(entt::registry& reg, entt::entity e, Shape s) {
    reg.remove<ShapeCircleTag, ShapeSquareTag, ShapeTriangleTag, ShapeHexagonTag>(e);
    const int idx = shape_index(s);
    if (idx < 0) return;
    shape_tag_detail::kEmplacePlayer[static_cast<size_t>(idx)](reg, e);
}

// Sets the player entity's shape-window target tag. Mechanics mirror
// `set_player_shape_tag`. The legacy `target_shape = Shape::Hexagon`
// "no playable shape" reset is encoded as `TargetShapeHexagonTag` presence.
inline void set_target_shape_tag(entt::registry& reg, entt::entity e, Shape s) {
    reg.remove<TargetShapeCircleTag, TargetShapeSquareTag,
               TargetShapeTriangleTag, TargetShapeHexagonTag>(e);
    const int idx = shape_index(s);
    if (idx < 0) return;
    shape_tag_detail::kEmplaceTarget[static_cast<size_t>(idx)](reg, e);
}

// Sets an obstacle entity's required-shape tag. Mechanics mirror
// `set_player_shape_tag`. Obstacle factories use this after their kind tag
// (ShapeGateTag / SplitPathTag) is emplaced.
inline void set_required_shape_tag(entt::registry& reg, entt::entity e, Shape s) {
    reg.remove<RequiredShapeCircleTag, RequiredShapeSquareTag,
               RequiredShapeTriangleTag, RequiredShapeHexagonTag>(e);
    const int idx = shape_index(s);
    if (idx < 0) return;
    shape_tag_detail::kEmplaceRequired[static_cast<size_t>(idx)](reg, e);
}

// Returns the Shape value matching the entity's current-shape tag. Returns
// `Shape::Hexagon` (the legacy default-state value) when no `Shape*Tag` is
// present — the player is treated as Hexagon-by-default both before the
// first shape press and after a window morph-out completes.
inline Shape current_player_shape(const entt::registry& reg, entt::entity e) noexcept {
    for (int i = 0; i < kShapeCount; ++i) {
        if (shape_tag_detail::kHasPlayer[static_cast<size_t>(i)](reg, e)) {
            return static_cast<Shape>(i);
        }
    }
    return Shape::Hexagon;
}

// Returns the Shape value matching the entity's shape-window target tag.
// Returns `Shape::Hexagon` when no `TargetShape*Tag` is present (matches
// the legacy "no target = Hexagon" sentinel).
inline Shape current_target_shape(const entt::registry& reg, entt::entity e) noexcept {
    for (int i = 0; i < kShapeCount; ++i) {
        if (shape_tag_detail::kHasTarget[static_cast<size_t>(i)](reg, e)) {
            return static_cast<Shape>(i);
        }
    }
    return Shape::Hexagon;
}

// Returns the Shape value matching the obstacle entity's required-shape tag.
// Returns `Shape::Hexagon` when no `RequiredShape*Tag` is present (legacy
// `RequiredShape == Shape::Hexagon` was rejected by collision unconditionally,
// so absence-of-tag yields the same matching behaviour).
inline Shape current_required_shape(const entt::registry& reg, entt::entity e) noexcept {
    for (int i = 0; i < kShapeCount; ++i) {
        if (shape_tag_detail::kHasRequired[static_cast<size_t>(i)](reg, e)) {
            return static_cast<Shape>(i);
        }
    }
    return Shape::Hexagon;
}

// True iff the obstacle entity carries any `RequiredShape*Tag`.
// Replaces the legacy `reg.try_get<RequiredShape>(e) != nullptr` probe.
inline bool has_required_shape_tag(const entt::registry& reg, entt::entity e) noexcept {
    return reg.any_of<RequiredShapeCircleTag, RequiredShapeSquareTag,
                      RequiredShapeTriangleTag, RequiredShapeHexagonTag>(e);
}
