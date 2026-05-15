#pragma once

#include <cstdint>
#include "tags/tags.h"
#include "window_phase.h"

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
// collision_system, and player input handlers.
struct ShapeWindow {
    Shape       target_shape  = Shape::Circle;
    WindowPhase phase         = WindowPhase::Idle;
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

enum class VMode : uint8_t {
    Grounded = 0,
    Jumping  = 1,
    Sliding  = 2
};

struct VerticalState {
    VMode mode     = VMode::Grounded;
    float timer    = 0.0f;
    float y_offset = 0.0f;
};
