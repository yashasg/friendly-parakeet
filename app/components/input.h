#pragma once

#include "player.h"
#include <cstdint>

struct InputState {
    bool  touch_down     = false;
    bool  touch_up       = false;
    bool  touching       = false;
    bool  quit_requested = false;

    float start_x = 0.0f, start_y = 0.0f;
    float curr_x  = 0.0f, curr_y  = 0.0f;
    float end_x   = 0.0f, end_y   = 0.0f;
    float duration = 0.0f;
};

inline void clear_input_events(InputState& input) {
    input.touch_down = false;
    input.touch_up   = false;
}

enum class Gesture : uint8_t {
    None       = 0,
    Tap        = 1,
    SwipeLeft  = 2,
    SwipeRight = 3,
    SwipeUp    = 4,
    SwipeDown  = 5
};

struct GestureResult {
    Gesture gesture   = Gesture::None;
    float   magnitude = 0.0f;
    float   hit_x     = 0.0f;
    float   hit_y     = 0.0f;
};

struct ShapeButtonEvent {
    bool  pressed = false;
    Shape shape   = Shape::Circle;
};
