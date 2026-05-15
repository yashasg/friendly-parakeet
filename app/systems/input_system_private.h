#pragma once

#include <entt/entt.hpp>

// System-private state for input_system. Stored in registry context, not
// emplaced on entities — this is per-system scratch, not component data.
// Extracted out of `InputState` (issue #1196). `InputState` itself was
// later relocated alongside this header from app/components/ to app/systems/
// (issue #1194).
//
//   * gestures_configured       — one-shot SetGesturesEnabled latch.
//   * was_focused               — previous-frame window focus, for edge detect.
//   * suppress_mouse_release    — pointer-capture latch that swallows the next
//                                 mouse release after a touch end so a touch
//                                 swipe does not also fire a stray click event.
//
// Reset via input_system_init() and input_system_clear_pointer_state().
struct InputSystemPrivate {
    bool gestures_configured = false;
    bool was_focused = true;
    bool suppress_mouse_release = false;
};

// Clears in-flight pointer-capture state so the next phase/screen starts from
// a clean slate. Called from game_state_system on phase transitions; the input
// system owns the reset because it owns the fields being cleared.
void input_system_clear_pointer_state(entt::registry& reg);
