#pragma once

#include "../components/input.h"

inline void clear_input_events(InputState& input) {
    input.touch_down = false;
    input.touch_up   = false;
}
