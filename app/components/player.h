#pragma once

#include <cstdint>
#include "tags/tags.h"

enum class Shape : uint8_t {
    Circle,
    Square,
    Triangle,
    Hexagon,
};

// Hot render data — read by game_camera_system, player_movement_system every frame.
struct PlayerShape {
    Shape current  = Shape::Circle;
    float morph_t  = 1.0f;
};

// Rhythm-mode shape window timing — read by shape_window_system,
// collision_system, and player input handlers. The window's phase
// (Idle/MorphIn/Active/MorphOut) lives as a per-phase tag on the player
// entity (`ShapeWindowMorphInTag` / `ShapeWindowActiveTag` /
// `ShapeWindowMorphOutTag` in `tags/tags.h`); Idle = absence of all three.
struct ShapeWindow {
    Shape       target_shape  = Shape::Circle;
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
