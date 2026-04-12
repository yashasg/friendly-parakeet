#pragma once

#include "player.h"
#include <cstdint>

// Tracks which input device initiated the current gesture so that
// mouse and touch events don't interfere on hybrid devices.
enum class InputSource : uint8_t { None, Mouse, Touch };

struct InputState {
    // ── Touch / pointer (all platforms) ──────────────────────
    bool  touch_down     = false;
    bool  touch_up       = false;
    bool  touching       = false;
    bool  quit_requested = false;

    float start_x  = 0.0f, start_y = 0.0f;
    float curr_x   = 0.0f, curr_y  = 0.0f;
    float end_x    = 0.0f, end_y   = 0.0f;
    float duration = 0.0f;

    InputSource active_source = InputSource::None;
    bool was_focused = true;  // edge detection for focus-loss auto-pause

#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_WEB)
    // ── Keyboard — one-frame pulse flags (desktop / web) ─────
    // Set to true by IsKeyPressed() in input_system,
    // cleared by clear_input_events() at the start of each frame.
    bool key_w = false;   // jump
    bool key_a = false;   // strafe left
    bool key_s = false;   // slide / crouch
    bool key_d = false;   // strafe right
    bool key_1 = false;   // shape: Circle
    bool key_2 = false;   // shape: Triangle
    bool key_3 = false;   // shape: Square
#endif
};

inline void clear_input_events(InputState& input) {
    input.touch_down = false;
    input.touch_up   = false;
#if defined(PLATFORM_DESKTOP) || defined(PLATFORM_WEB)
    input.key_w = false;
    input.key_a = false;
    input.key_s = false;
    input.key_d = false;
    input.key_1 = false;
    input.key_2 = false;
    input.key_3 = false;
#endif
}

enum class SwipeGesture : uint8_t {
    None       = 0,
    Tap        = 1,
    SwipeLeft  = 2,
    SwipeRight = 3,
    SwipeUp    = 4,
    SwipeDown  = 5
};

struct GestureResult {
    SwipeGesture gesture   = SwipeGesture::None;
    float   magnitude = 0.0f;
    float   hit_x     = 0.0f;
    float   hit_y     = 0.0f;
};

struct ShapeButtonEvent {
    bool  pressed = false;
    Shape shape   = Shape::Circle;
};
