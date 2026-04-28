#pragma once

#include "player.h"
#include <cstdint>

// ── Raw input state (internal to input_system) ──────────────────────────────
// Tracks touch/mouse hardware state. Downstream systems should read
// EventQueue, not this struct — except for quit_requested.

enum class InputSource : uint8_t { None, Mouse, Touch };

struct InputState {
    bool  touch_down     = false;
    bool  touch_up       = false;
    bool  touching       = false;
    bool  quit_requested = false;

    float start_x  = 0.0f, start_y = 0.0f;
    float curr_x   = 0.0f, curr_y  = 0.0f;
    float end_x    = 0.0f, end_y   = 0.0f;
    float duration = 0.0f;

    InputSource active_source = InputSource::None;
    bool was_focused = true;
    bool gestures_configured = false;
};

// ── Directions ──────────────────────────────────────────────────────────────
// Shared by InputEvent routing, hit-test helpers, and consumer systems.

enum class Direction : uint8_t { Left, Right, Up, Down };
