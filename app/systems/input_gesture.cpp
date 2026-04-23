#include "input_gesture.h"
#include "../constants.h"
#include <cmath>

void enqueue_pointer_release_action(ActionQueue& aq, const InputState& input) {
    const float zone_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;

    if (input.start_y >= zone_y) {
        aq.click(input.end_x, input.end_y);
        return;
    }

    const float dx = input.end_x - input.start_x;
    const float dy = input.end_y - input.start_y;
    const float dist = std::sqrt(dx * dx + dy * dy);

    if (dist >= constants::MIN_SWIPE_DIST && input.duration <= constants::MAX_SWIPE_TIME) {
        if (std::abs(dx) > std::abs(dy)) {
            aq.go(dx > 0 ? Direction::Right : Direction::Left);
        } else {
            aq.go(dy > 0 ? Direction::Down : Direction::Up);
        }
        return;
    }

    aq.click(input.end_x, input.end_y);
}
