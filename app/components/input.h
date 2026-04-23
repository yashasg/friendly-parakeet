#pragma once

#include "player.h"
#include "../platform.h"
#include <cstdint>
#include <optional>

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

// Maps a tap-button to the corresponding player Shape when the button
// represents a shape morph (ShapeCircle/Square/Tri). Returns std::nullopt for
// non-shape buttons (e.g. Confirm).
[[nodiscard]] inline std::optional<Shape> shape_from_button(Button b) {
    switch (b) {
        case Button::ShapeCircle: return Shape::Circle;
        case Button::ShapeSquare: return Shape::Square;
        case Button::ShapeTri:    return Shape::Triangle;
        default:                  return std::nullopt;
    }
}

// Inverse of shape_from_button: returns the tap-button that morphs the
// player into the given shape. Returns std::nullopt for shapes that have
// no dedicated button (e.g. Shape::Hexagon is the neutral/idle state).
[[nodiscard]] inline std::optional<Button> button_from_shape(Shape s) {
    switch (s) {
        case Shape::Circle:   return Button::ShapeCircle;
        case Shape::Square:   return Button::ShapeSquare;
        case Shape::Triangle: return Button::ShapeTri;
        default:              return std::nullopt;
    }
}

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
