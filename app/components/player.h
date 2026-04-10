#pragma once

#include <cstdint>

enum class Shape : uint8_t {
    Circle   = 0,
    Square   = 1,
    Triangle = 2,
    Hexagon  = 3
};

struct PlayerTag {};

// Forward-declared in rhythm.h; duplicated here to keep player.h self-contained.
enum class WindowPhase : uint8_t;

struct PlayerShape {
    Shape current  = Shape::Circle;
    Shape previous = Shape::Circle;
    float morph_t  = 1.0f;

    // Window tracking (rhythm mode)
    Shape target_shape  = Shape::Circle;
    uint8_t phase_raw   = 0;       // WindowPhase stored as uint8_t to avoid circular include
    float window_timer  = 0.0f;    // elapsed time in current phase
    float window_start  = 0.0f;    // song_time when window was triggered
    float peak_time     = 0.0f;    // song_time of window peak

    // Window scaling (set after collision graded)
    float window_scale  = 1.0f;    // 1.0 = full, 0.5 = PERFECT snap
    bool  graded        = false;   // true once this window's obstacle has been scored
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
