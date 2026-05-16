#pragma once

// ── Raw input state (internal to input_system) ──────────────────────────────
// Tracks touch/mouse hardware state. Downstream systems should read
// semantic events, not this struct — except for quit_requested.
//
// Pure system-private fields (gestures_configured, was_focused,
// suppress_mouse_release) live in InputSystemPrivate (see
// app/systems/input_system_private.h) — issue #1196.
//
// Relocated out of app/components/ alongside the input system itself
// (issue #1194) — this is per-frame ctx-singleton hardware-capture state,
// not entity-owned component data.

// Per-source ctx tags replacing the former `InputSource` enum
// (issues #1202 / #1204). The former 3-state mutex
// (`None`/`Mouse`/`Touch`) on `InputState::active_source` becomes:
//   • presence of `InputSourceMouse` ctx table  ⇔ Mouse owns the gesture
//   • presence of `InputSourceTouch` ctx table  ⇔ Touch owns the gesture
//   • absence of both                            ⇔ None
// The mutex (at most one tag present) is enforced by the input_system
// helpers `set_input_source_mouse / _touch` and `clear_input_source`.
struct InputSourceMouse {};
struct InputSourceTouch {};

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

    bool  touch_down     = false;
    bool  touch_up       = false;
    bool  click          = false;
    bool  touching       = false;
    bool  quit_requested = false;
    bool button_touch_up = false;
    float button_end_x = 0.0f, button_end_y = 0.0f;
    TouchSlot touch_slots[MaxTrackedTouches] = {};
};

// Note: `Direction` lives in app/systems/input_events.h alongside `GoEvent`,
// the only event that carries it (issue #1194).
