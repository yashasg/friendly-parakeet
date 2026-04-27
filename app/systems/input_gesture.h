#pragma once

#include "../components/input_events.h"
#include "../constants.h"
#include <cmath>

// Pure gesture classification helper with no raylib dependency.
// Classifies a touch release into an InputEvent based on position and duration.
// This is the canonical logic used by input_system and tested independently.

inline InputEvent classify_touch_release(
    float start_x, float start_y,
    float end_x, float end_y,
    float duration_sec
) {
    float zone_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;

    if (start_y >= zone_y) {
        // Button zone (bottom 20%) always emits a tap at end coordinates.
        return InputEvent{InputType::Tap, Direction::Up, end_x, end_y};
    } else {
        float dx = end_x - start_x;
        float dy = end_y - start_y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist >= constants::MIN_SWIPE_DIST && duration_sec <= constants::MAX_SWIPE_TIME) {
            Direction dir;
            if (std::abs(dx) > std::abs(dy)) {
                dir = dx > 0 ? Direction::Right : Direction::Left;
            } else {
                // Equal magnitude falls through to vertical, matching input_system history.
                dir = dy > 0 ? Direction::Down : Direction::Up;
            }
            return InputEvent{InputType::Swipe, dir, end_x, end_y};
        } else {
            return InputEvent{InputType::Tap, Direction::Up, end_x, end_y};
        }
    }
}
