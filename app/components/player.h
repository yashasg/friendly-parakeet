#pragma once

#include <cstdint>
#include <string_view>
#include "tags/tags.h"

enum class Shape : uint8_t {
    Circle,
    Square,
    Triangle,
    Hexagon,
};

// Stringification for trace/log lines. Pure label lookup, doctrinally fine
// per Fabian's Existential Processing chapter (see issue #1204 — "Bonus:
// drop the magic_enum dependency"). Replaces the former
// `magic_enum::enum_name(Shape)` lookup used by the session logger and tests.
constexpr std::string_view to_string(Shape shape) noexcept {
    switch (shape) {
        case Shape::Circle:   return "Circle";
        case Shape::Square:   return "Square";
        case Shape::Triangle: return "Triangle";
        case Shape::Hexagon:  return "Hexagon";
    }
    return {};
}

// Hot render data — read by game_camera_system, player_movement_system every frame.
// The player's current shape identity lives as one of `ShapeCircleTag` /
// `ShapeSquareTag` / `ShapeTriangleTag` / `ShapeHexagonTag` on the player
// entity (issue #1202/#1204); see `app/util/shape_tag.h` for the helper
// dispatch and the rationale for the per-tag table mechanic.
struct PlayerShape {
    float morph_t  = 1.0f;
};

// Rhythm-mode shape window timing — read by shape_window_system,
// collision_system, and player input handlers. The window's phase
// (Idle/MorphIn/Active/MorphOut) lives as a per-phase tag on the player
// entity (`ShapeWindowMorphInTag` / `ShapeWindowActiveTag` /
// `ShapeWindowMorphOutTag` in `tags/tags.h`); Idle = absence of all three.
// The target shape the press is morphing toward lives as one of
// `TargetShapeCircleTag` / `TargetShapeSquareTag` / `TargetShapeTriangleTag` /
// `TargetShapeHexagonTag` on the same entity (issue #1202/#1204).
struct ShapeWindow {
    bool        graded        = false;
    float       window_timer  = 0.0f;
    float       window_start  = 0.0f;
    float       press_time    = -1.0f;
};

struct Lane {
    int8_t current = 1;
    int8_t target  = -1;
    float  lerp_t  = 1.0f;
};

// ── Vertical motion state (per-state component tables) ──────
// Per Fabian's existential processing (issue #1202/#1204), each former
// VMode value becomes its own component table on the player entity:
//   - `Jumping` present  → entity is mid-jump  (timer + parabolic y_offset)
//   - `Sliding` present  → entity is mid-slide (timer; y_offset always 0,
//     visual squash is handled in the render system)
//   - Absence of both    → grounded (default state — no data needed)
//
// State transitions are table operations:
//   start jump:  reg.emplace<Jumping>(e, {JUMP_DURATION, 0.0f});
//   end jump:    reg.remove<Jumping>(e);
// `player_movement_system` splits per-state ticks (jumping / sliding)
// into separate transforms operating on their own filtered views.
struct Jumping {
    float timer    = 0.0f;
    float y_offset = 0.0f;
};

struct Sliding {
    float timer = 0.0f;
};
