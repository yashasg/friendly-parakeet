#pragma once

#include "player.h"
#include "../platform.h"
#include <cstdint>

// ── Raw input state (internal to input_system) ──────────────────────────────
// Tracks touch/mouse hardware state. Downstream systems should read
// ActionQueue, not this struct — except for quit_requested.

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
};

inline void clear_input_events(InputState& input) {
    input.touch_down = false;
    input.touch_up   = false;
}

// ── Player Actions ──────────────────────────────────────────────────────────
// All input (keyboard, mouse, touch, swipe) maps to one of three verbs:
//   Go(Direction)   — directional intent (lane change, menu navigate)
//   Tap(Button)     — semantic button intent (shape morph, confirm)
//   Click(x, y)     — positional click/tap intent for UI hit-box resolution
//
// The input system is the sole producer. All other systems are consumers.

enum class Direction : uint8_t { Left, Right, Up, Down };

enum class Button : uint8_t {
    ShapeCircle  = 0,
    ShapeSquare  = 1,
    ShapeTri     = 2,
    Confirm      = 3,
};

enum class ActionVerb : uint8_t { Go, Tap, Click };

struct PlayerAction {
    ActionVerb verb = ActionVerb::Go;
    union {
        Direction dir;
        Button    button;
    };
    float x = 0.0f, y = 0.0f;
};

struct ActionQueue {
    static constexpr int MAX = 8;
    PlayerAction actions[MAX] = {};
    int count = 0;

    void go(Direction d) {
        if (count < MAX) {
            auto& a = actions[count++];
            a.verb = ActionVerb::Go;
            a.dir = d;
            a.x = 0.0f; a.y = 0.0f;
        }
    }
    void tap(Button b, float px = 0.0f, float py = 0.0f) {
        if (count < MAX) {
            auto& a = actions[count++];
            a.verb = ActionVerb::Tap;
            a.button = b;
            a.x = px; a.y = py;
        }
    }
    void click(float px, float py) {
        if (count < MAX) {
            auto& a = actions[count++];
            a.verb = ActionVerb::Click;
            a.x = px; a.y = py;
        }
    }
    void clear() { count = 0; }
};
