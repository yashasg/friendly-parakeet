#pragma once

#include "../components/input_events.h"
#include "../constants.h"
#include <raylib.h>

constexpr unsigned int kGameplayGestureFlags =
    GESTURE_TAP
    | GESTURE_SWIPE_RIGHT
    | GESTURE_SWIPE_LEFT
    | GESTURE_SWIPE_UP
    | GESTURE_SWIPE_DOWN;

inline int read_detected_raylib_gesture() {
    if (IsGestureDetected(GESTURE_SWIPE_RIGHT)) return GESTURE_SWIPE_RIGHT;
    if (IsGestureDetected(GESTURE_SWIPE_LEFT))  return GESTURE_SWIPE_LEFT;
    if (IsGestureDetected(GESTURE_SWIPE_UP))    return GESTURE_SWIPE_UP;
    if (IsGestureDetected(GESTURE_SWIPE_DOWN))  return GESTURE_SWIPE_DOWN;
    if (IsGestureDetected(GESTURE_TAP))         return GESTURE_TAP;
    return GetGestureDetected();
}

inline InputEvent input_event_from_raylib_gesture(
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
