#include "shape_button_hit.h"
#include "../constants.h"

std::optional<Shape> shape_button_hit_test(float x, float y) {
    constexpr float BUTTON_RADIUS_DIVISOR      = 2.8f;
    constexpr float BUTTON_HIT_RADIUS_MULTIPLIER = 1.4f;

    const float btn_w       = constants::BUTTON_W_N * constants::SCREEN_W;
    const float btn_h       = constants::BUTTON_H_N * constants::SCREEN_H;
    const float btn_spacing = constants::BUTTON_SPACING_N * constants::SCREEN_W;
    const float btn_y       = constants::BUTTON_Y_N * constants::SCREEN_H;

    const float btn_area_x_start = (constants::SCREEN_W - 3.0f * btn_w - 2.0f * btn_spacing) / 2.0f;
    const float btn_cy           = btn_y + btn_h / 2.0f;
    const float btn_radius       = btn_w / BUTTON_RADIUS_DIVISOR;
    const float hit_radius       = btn_radius * BUTTON_HIT_RADIUS_MULTIPLIER;

    for (int i = 0; i < 3; ++i) {
        const float btn_cx = btn_area_x_start + static_cast<float>(i) * (btn_w + btn_spacing) + btn_w / 2.0f;
        const float dx = x - btn_cx;
        const float dy = y - btn_cy;
        if (dx * dx + dy * dy <= hit_radius * hit_radius) {
            return static_cast<Shape>(i);
        }
    }

    return std::nullopt;
}
