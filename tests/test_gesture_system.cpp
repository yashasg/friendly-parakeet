#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

TEST_CASE("gesture_system: no gesture when no touch_up", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    input.touch_up = false;

    gesture_system(reg, 0.016f);

    auto& gesture = reg.ctx().get<GestureResult>();
    CHECK(gesture.gesture == Gesture::None);
    CHECK_FALSE(reg.ctx().get<ShapeButtonEvent>().pressed);
}

TEST_CASE("gesture_system: swipe right detected", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    input.touch_up = true;
    input.start_x = 100.0f;
    input.start_y = 400.0f;  // in swipe zone (top 80%)
    input.end_x   = 250.0f;  // dx = 150, well above MIN_SWIPE_DIST
    input.end_y   = 400.0f;
    input.duration = 0.1f;   // under MAX_SWIPE_TIME

    gesture_system(reg, 0.016f);

    auto& gesture = reg.ctx().get<GestureResult>();
    CHECK(gesture.gesture == Gesture::SwipeRight);
    CHECK(gesture.magnitude > constants::MIN_SWIPE_DIST);
}

TEST_CASE("gesture_system: swipe left detected", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    input.touch_up = true;
    input.start_x = 400.0f;
    input.start_y = 300.0f;
    input.end_x   = 200.0f;  // dx = -200
    input.end_y   = 300.0f;
    input.duration = 0.15f;

    gesture_system(reg, 0.016f);

    CHECK(reg.ctx().get<GestureResult>().gesture == Gesture::SwipeLeft);
}

TEST_CASE("gesture_system: swipe up detected", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    input.touch_up = true;
    input.start_x = 360.0f;
    input.start_y = 500.0f;
    input.end_x   = 360.0f;
    input.end_y   = 300.0f;  // dy = -200 (up on screen)
    input.duration = 0.1f;

    gesture_system(reg, 0.016f);

    CHECK(reg.ctx().get<GestureResult>().gesture == Gesture::SwipeUp);
}

TEST_CASE("gesture_system: swipe down detected", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    input.touch_up = true;
    input.start_x = 360.0f;
    input.start_y = 300.0f;
    input.end_x   = 360.0f;
    input.end_y   = 500.0f;  // dy = 200 (down on screen)
    input.duration = 0.1f;

    gesture_system(reg, 0.016f);

    CHECK(reg.ctx().get<GestureResult>().gesture == Gesture::SwipeDown);
}

TEST_CASE("gesture_system: swipe too short is ignored", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    input.touch_up = true;
    input.start_x = 360.0f;
    input.start_y = 400.0f;
    input.end_x   = 370.0f;  // dx = 10, below MIN_SWIPE_DIST
    input.end_y   = 400.0f;
    input.duration = 0.1f;

    gesture_system(reg, 0.016f);

    CHECK(reg.ctx().get<GestureResult>().gesture == Gesture::None);
}

TEST_CASE("gesture_system: swipe too slow is ignored", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    input.touch_up = true;
    input.start_x = 100.0f;
    input.start_y = 400.0f;
    input.end_x   = 300.0f;
    input.end_y   = 400.0f;
    input.duration = 0.5f;  // above MAX_SWIPE_TIME

    gesture_system(reg, 0.016f);

    CHECK(reg.ctx().get<GestureResult>().gesture == Gesture::None);
}

TEST_CASE("gesture_system: button tap in button zone", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    float zone_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;

    // Tap in the button zone on the second button (Square)
    float btn_area_x_start = (constants::SCREEN_W
        - 3 * constants::BUTTON_W
        - 2 * constants::BUTTON_SPACING) / 2.0f;
    float btn2_center = btn_area_x_start
        + 1 * (constants::BUTTON_W + constants::BUTTON_SPACING)
        + constants::BUTTON_W / 2.0f;

    input.touch_up = true;
    input.start_y  = zone_y + 10.0f;  // in button zone
    input.end_x    = btn2_center;
    input.end_y    = zone_y + 10.0f;
    input.duration = 0.05f;

    gesture_system(reg, 0.016f);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    CHECK(btn.pressed);
    CHECK(btn.shape == Shape::Square);
    CHECK(reg.ctx().get<GestureResult>().gesture == Gesture::None);
}

TEST_CASE("gesture_system: button tap on first button (Circle)", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    float zone_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;

    float btn_area_x_start = (constants::SCREEN_W
        - 3 * constants::BUTTON_W
        - 2 * constants::BUTTON_SPACING) / 2.0f;
    float btn1_center = btn_area_x_start + constants::BUTTON_W / 2.0f;

    input.touch_up = true;
    input.start_y  = zone_y + 10.0f;
    input.end_x    = btn1_center;
    input.end_y    = zone_y + 10.0f;
    input.duration = 0.05f;

    gesture_system(reg, 0.016f);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    CHECK(btn.pressed);
    CHECK(btn.shape == Shape::Circle);
}

TEST_CASE("gesture_system: tap outside buttons in button zone", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    float zone_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;

    input.touch_up = true;
    input.start_y  = zone_y + 10.0f;
    input.end_x    = 5.0f;  // far left, not on any button
    input.end_y    = zone_y + 10.0f;

    gesture_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<ShapeButtonEvent>().pressed);
}
