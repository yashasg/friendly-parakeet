#pragma once

#include "../components/input_events.h"
#include "../constants.h"
#include "runtime/runtime_api.h"

inline InputEvent input_event_from_detected_gesture(
    int gesture,
    float start_y,
    float end_x,
    float end_y
) {
    const float zone_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;
    if (start_y >= zone_y) {
        return InputEvent{InputType::Tap, Direction::Up, end_x, end_y};
    }

    if ((gesture & GESTURE_SWIPE_RIGHT) != 0) {
        return InputEvent{InputType::Swipe, Direction::Right, end_x, end_y};
    }
    if ((gesture & GESTURE_SWIPE_LEFT) != 0) {
        return InputEvent{InputType::Swipe, Direction::Left, end_x, end_y};
    }
    if ((gesture & GESTURE_SWIPE_UP) != 0) {
        return InputEvent{InputType::Swipe, Direction::Up, end_x, end_y};
    }
    if ((gesture & GESTURE_SWIPE_DOWN) != 0) {
        return InputEvent{InputType::Swipe, Direction::Down, end_x, end_y};
    }

    return InputEvent{InputType::Tap, Direction::Up, end_x, end_y};
}
