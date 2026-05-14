#pragma once

#include <cstdint>

// ── Raw input state (internal to input_system) ──────────────────────────────
// Tracks touch/mouse hardware state. Downstream systems should read
// semantic events, not this struct — except for quit_requested.

enum class InputSource : uint8_t { None, Mouse, Touch };

struct TouchSlot {
    static constexpr int InvalidId = -1;

    int id = InvalidId;
    bool active = false;
    bool started_in_button_zone = false;
    float start_x = 0.0f, start_y = 0.0f;
    float curr_x = 0.0f, curr_y = 0.0f;
    float duration = 0.0f;
};

struct InputState {
    static constexpr int MaxTrackedTouches = 2;

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
    bool suppress_mouse_release = false;
    bool button_touch_up = false;
    float button_end_x = 0.0f, button_end_y = 0.0f;
    TouchSlot touch_slots[MaxTrackedTouches] = {};
};

// ── Directions ──────────────────────────────────────────────────────────────
// Shared by semantic input producers and consumers.

enum class Direction : uint8_t { Left, Right, Up, Down };
