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
// The mutex (at most one tag present) is enforced inline at the two set
// sites in input_system (insert one tag + erase the sibling) and via the
// shared `clear_input_source` helper which drops both tags.
struct InputSourceMouse {};
struct InputSourceTouch {};

// One row per currently-tracked finger (issue #1612 / Fabian Principle 3).
// Presence in `reg.view<ActiveTouchSlot>()` IS membership in "currently
// tracked"; the parent `InputState` no longer carries a fixed-size
// `TouchSlot touch_slots[MaxTrackedTouches]` array column nor the
// `id == InvalidId = -1` NULL column that gated it. The
// `InputState::MaxTrackedTouches` policy cap survives as a runtime
// allocation guard at the create site in input_system. The `id` field
// is the raylib touch id used to match this row against the live
// `GetTouchPointId(i)` table each frame — it is a real attribute of an
// active touch, not a sentinel.
struct ActiveTouchSlot {
    int   id;
    bool  started_in_button_zone;
    float start_x, start_y;
    float curr_x,  curr_y;
    float duration;
};

struct InputState {
    // Runtime cap on concurrent `ActiveTouchSlot` rows. Enforced at the
    // create site in input_system; no longer the dim of an inline array.
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
};

// Note: directional intent is identity-encoded in per-direction event types
// (`GoUpEvent`, `GoDownEvent`, `GoLeftEvent`, `GoRightEvent`) declared in
// app/systems/input_events.h — see issue #1279 (the former `Direction` enum
// and `GoEvent { Direction dir }` were eradicated).
