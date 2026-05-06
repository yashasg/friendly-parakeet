#pragma once

#include <cstdint>

// ── Raw input state (internal to input_system) ──────────────────────────────
// Tracks touch/mouse hardware state. Downstream systems should read
// EventQueue, not this struct — except for quit_requested.

enum class InputSource : uint8_t { None, Mouse, Touch };

struct InputState {
    float start_x  = 0.0f, start_y = 0.0f;
    float curr_x   = 0.0f, curr_y  = 0.0f;
    float end_x    = 0.0f, end_y   = 0.0f;
    float duration = 0.0f;

    InputSource active_source = InputSource::None;
    bool  touch_down     = false;
    bool  touch_up       = false;
    bool  click          = false;
    bool  touching       = false;
    bool  quit_requested = false;
    bool was_focused = true;
    bool gestures_configured = false;
};

inline void clear_transient_input_flags(InputState& input) noexcept {
    input.touch_down = false;
    input.touch_up = false;
    input.click = false;
}

inline void reset_pointer_capture(InputState& input) noexcept {
    clear_transient_input_flags(input);
    input.touching = false;
    input.active_source = InputSource::None;
    input.duration = 0.0f;
}

// ── Directions ──────────────────────────────────────────────────────────────
// Shared by InputEvent routing and consumer systems.

enum class Direction : uint8_t { Left, Right, Up, Down };
