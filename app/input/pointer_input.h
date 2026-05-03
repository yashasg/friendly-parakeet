#pragma once

#include "../components/input.h"
#include <raylib.h>

// Unified pointer-release surface used by UI click handlers.
// InputState is already normalized to virtual-space coordinates by input_system.
inline bool pointer_release_position(const InputState& input, Vector2& out_pos) {
    if (input.click || input.touch_up) {
        out_pos = {input.end_x, input.end_y};
        return true;
    }
    return false;
}
