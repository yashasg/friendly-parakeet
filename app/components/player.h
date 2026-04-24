#pragma once

#include <cstdint>

#define SHAPE_LIST(X) \
    X(Circle)         \
    X(Square)         \
    X(Triangle)       \
    X(Hexagon)

enum class Shape : uint8_t {
    #define SHAPE_ENUM(name) name,
    SHAPE_LIST(SHAPE_ENUM)
    #undef SHAPE_ENUM
};

inline const char* ToString(Shape s) {
    switch (s) {
        #define SHAPE_STR(name) case Shape::name: return #name;
        SHAPE_LIST(SHAPE_STR)
        #undef SHAPE_STR
    }
    return "???";
}

struct PlayerTag {};

// Forward-declared in rhythm.h; duplicated here to keep player.h self-contained.
enum class WindowPhase : uint8_t;

// Hot render data — read by render_system, player_movement_system every frame.
struct PlayerShape {
    Shape current  = Shape::Circle;
    Shape previous = Shape::Circle;
    float morph_t  = 1.0f;
};

// Rhythm-mode shape window timing — read by shape_window_system,
// collision_system, player_action_system.
struct ShapeWindow {
    Shape   target_shape  = Shape::Circle;
    uint8_t phase_raw     = 0;       // WindowPhase stored as uint8_t
    float   window_timer  = 0.0f;
    float   window_start  = 0.0f;
    float   peak_time     = 0.0f;
    float   window_scale  = 1.0f;
    bool    graded        = false;
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

#undef SHAPE_LIST
