#include "all_systems.h"
#include "../components/input.h"
#include "../constants.h"
#include <cmath>

void gesture_system(entt::registry& reg, float /*dt*/) {
    auto& input   = reg.ctx().get<InputState>();
    auto& gesture = reg.ctx().get<GestureResult>();
    auto& btn_evt = reg.ctx().get<ShapeButtonEvent>();

    gesture.gesture = Gesture::None;
    btn_evt.pressed = false;

    if (!input.touch_up) return;

    float zone_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;

    // Button zone: tap detected in bottom 20%
    if (input.start_y >= zone_y) {
        float btn_area_x_start = (constants::SCREEN_W
            - 3 * constants::BUTTON_W
            - 2 * constants::BUTTON_SPACING) / 2.0f;

        for (int i = 0; i < 3; ++i) {
            float bx = btn_area_x_start
                + static_cast<float>(i) * (constants::BUTTON_W + constants::BUTTON_SPACING);
            if (input.end_x >= bx && input.end_x <= bx + constants::BUTTON_W) {
                btn_evt.pressed = true;
                btn_evt.shape = static_cast<Shape>(i);
                return;
            }
        }
        return;
    }

    // Swipe zone: classify direction
    float dx = input.end_x - input.start_x;
    float dy = input.end_y - input.start_y;
    float dist = std::sqrt(dx * dx + dy * dy);

    if (dist < constants::MIN_SWIPE_DIST || input.duration > constants::MAX_SWIPE_TIME) {
        return; // too short or too slow
    }

    gesture.hit_x     = input.start_x;
    gesture.hit_y     = input.start_y;
    gesture.magnitude = dist;

    if (std::abs(dx) > std::abs(dy)) {
        gesture.gesture = (dx > 0) ? Gesture::SwipeRight : Gesture::SwipeLeft;
    } else {
        gesture.gesture = (dy > 0) ? Gesture::SwipeDown : Gesture::SwipeUp;
    }
}
