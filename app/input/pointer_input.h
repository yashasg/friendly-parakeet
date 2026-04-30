#pragma once

#include "../components/input.h"
#include <raylib.h>

// Unified pointer-release surface used by UI click handlers.
// InputState is already normalized to virtual-space coordinates by input_system.
inline bool pointer_release_position(const InputState& input, Vector2& out_pos) {
    if (input.touch_up) {
        out_pos = {input.end_x, input.end_y};
        return true;
    }
    return false;
}

// Title-screen activation helper:
// - canonical release edge across platforms
// - web desktop fallback for stacks that miss release while still reporting press
inline bool pointer_activate_position(const InputState& input, Vector2& out_pos) {
    if (pointer_release_position(input, out_pos)) return true;
#ifdef PLATFORM_WEB
    if (input.touch_down && input.active_source == InputSource::Mouse) {
        out_pos = {input.start_x, input.start_y};
        return true;
    }
#endif
    return false;
}
