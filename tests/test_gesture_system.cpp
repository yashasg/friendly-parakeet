#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

TEST_CASE("gesture_system: no gesture when no touch_up", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    input.touch_up = false;

    gesture_system(reg, 0.016f);

    auto& gesture = reg.ctx().get<GestureResult>();
    CHECK(gesture.gesture == SwipeGesture::None);
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
    CHECK(gesture.gesture == SwipeGesture::SwipeRight);
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

    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::SwipeLeft);
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

    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::SwipeUp);
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

    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::SwipeDown);
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

    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::None);
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

    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::None);
}

TEST_CASE("gesture_system: button tap in button zone", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();

    // Tap in the button zone on the second button (Square)
    float btn_area_x_start = (constants::SCREEN_W
        - 3 * constants::BUTTON_W
        - 2 * constants::BUTTON_SPACING) / 2.0f;
    float btn2_center = btn_area_x_start
        + 1 * (constants::BUTTON_W + constants::BUTTON_SPACING)
        + constants::BUTTON_W / 2.0f;

    input.touch_up = true;
    input.start_y  = constants::BUTTON_Y + constants::BUTTON_H / 2.0f;  // in button zone
    input.end_x    = btn2_center;
    input.end_y    = constants::BUTTON_Y + constants::BUTTON_H / 2.0f;
    input.duration = 0.05f;

    gesture_system(reg, 0.016f);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    CHECK(btn.pressed);
    CHECK(btn.shape == Shape::Square);
    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::None);
}

TEST_CASE("gesture_system: button tap on first button (Circle)", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();

    float btn_area_x_start = (constants::SCREEN_W
        - 3 * constants::BUTTON_W
        - 2 * constants::BUTTON_SPACING) / 2.0f;
    float btn1_center = btn_area_x_start + constants::BUTTON_W / 2.0f;

    input.touch_up = true;
    input.start_y  = constants::BUTTON_Y + constants::BUTTON_H / 2.0f;
    input.end_x    = btn1_center;
    input.end_y    = constants::BUTTON_Y + constants::BUTTON_H / 2.0f;
    input.duration = 0.05f;

    gesture_system(reg, 0.016f);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    CHECK(btn.pressed);
    CHECK(btn.shape == Shape::Circle);
}

TEST_CASE("gesture_system: tap outside buttons in button zone", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();

    input.touch_up = true;
    input.start_y  = constants::BUTTON_Y + constants::BUTTON_H / 2.0f;
    input.end_x    = 5.0f;  // far left, not on any button
    input.end_y    = constants::BUTTON_Y + constants::BUTTON_H / 2.0f;

    gesture_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<ShapeButtonEvent>().pressed);
}

TEST_CASE("gesture_system: swipe sets magnitude and hit coordinates", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    input.touch_up = true;
    input.start_x = 200.0f;
    input.start_y = 300.0f;
    input.end_x   = 400.0f;  // dx = 200
    input.end_y   = 300.0f;
    input.duration = 0.1f;

    gesture_system(reg, 0.016f);

    auto& g = reg.ctx().get<GestureResult>();
    CHECK(g.gesture == SwipeGesture::SwipeRight);
    CHECK(g.hit_x == 200.0f);  // start coords
    CHECK(g.hit_y == 300.0f);
    CHECK(g.magnitude > 0.0f);
}

TEST_CASE("gesture_system: diagonal swipe classified by dominant axis", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    input.touch_up = true;
    input.start_x = 200.0f;
    input.start_y = 200.0f;
    // More horizontal than vertical
    input.end_x   = 400.0f;  // dx = 200
    input.end_y   = 250.0f;  // dy = 50
    input.duration = 0.1f;

    gesture_system(reg, 0.016f);

    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::SwipeRight);
}

TEST_CASE("gesture_system: button tap on third button (Triangle)", "[gesture]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();

    float btn_area_x_start = (constants::SCREEN_W
        - 3 * constants::BUTTON_W
        - 2 * constants::BUTTON_SPACING) / 2.0f;
    float btn3_center = btn_area_x_start
        + 2.0f * (constants::BUTTON_W + constants::BUTTON_SPACING)
        + constants::BUTTON_W / 2.0f;

    input.touch_up = true;
    input.start_y  = constants::BUTTON_Y + constants::BUTTON_H / 2.0f;
    input.end_x    = btn3_center;
    input.end_y    = constants::BUTTON_Y + constants::BUTTON_H / 2.0f;
    input.duration = 0.05f;

    gesture_system(reg, 0.016f);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    CHECK(btn.pressed);
    CHECK(btn.shape == Shape::Triangle);
}

#ifdef PLATFORM_DESKTOP
// ── Desktop input tests ───────────────────────────────────────────────
// Left-click on shape buttons must continue to work on desktop
// (mouse events are mapped to touch, keyboard block is a no-op when no key is held).

TEST_CASE("gesture_system: left-click on Circle button works on desktop", "[gesture][desktop]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    float btn_area_x_start = (constants::SCREEN_W
        - 3 * constants::BUTTON_W
        - 2 * constants::BUTTON_SPACING) / 2.0f;
    float btn0_center = btn_area_x_start + constants::BUTTON_W / 2.0f;

    // Simulate SDL_MOUSEBUTTONUP mapped to touch_up (no key pressed)
    input.touch_up = true;
    input.start_y  = constants::BUTTON_Y + constants::BUTTON_H / 2.0f;
    input.end_x    = btn0_center;
    input.end_y    = constants::BUTTON_Y + constants::BUTTON_H / 2.0f;
    input.duration = 0.05f;

    gesture_system(reg, 0.016f);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    CHECK(btn.pressed);
    CHECK(btn.shape == Shape::Circle);
    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::None);
}

TEST_CASE("gesture_system: left-click on Triangle button works on desktop", "[gesture][desktop]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    float btn_area_x_start = (constants::SCREEN_W
        - 3 * constants::BUTTON_W
        - 2 * constants::BUTTON_SPACING) / 2.0f;
    // Triangle is the third button (index 2) — Shape::Triangle == 2
    float btn2_center = btn_area_x_start
        + 2.0f * (constants::BUTTON_W + constants::BUTTON_SPACING)
        + constants::BUTTON_W / 2.0f;

    input.touch_up = true;
    input.start_y  = constants::BUTTON_Y + constants::BUTTON_H / 2.0f;
    input.end_x    = btn2_center;
    input.end_y    = constants::BUTTON_Y + constants::BUTTON_H / 2.0f;
    input.duration = 0.05f;

    gesture_system(reg, 0.016f);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    CHECK(btn.pressed);
    CHECK(btn.shape == Shape::Triangle);
}

// ── Keyboard gesture tests (desktop only) ────────────────────────────

TEST_CASE("gesture_system: W key fires SwipeUp", "[gesture][desktop]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    input.key_w = true;

    gesture_system(reg, 0.016f);

    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::SwipeUp);
    CHECK_FALSE(reg.ctx().get<ShapeButtonEvent>().pressed);
}

TEST_CASE("gesture_system: S key fires SwipeDown", "[gesture][desktop]") {
    auto reg = make_registry();
    reg.ctx().get<InputState>().key_s = true;

    gesture_system(reg, 0.016f);

    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::SwipeDown);
}

TEST_CASE("gesture_system: A key fires SwipeLeft", "[gesture][desktop]") {
    auto reg = make_registry();
    reg.ctx().get<InputState>().key_a = true;

    gesture_system(reg, 0.016f);

    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::SwipeLeft);
}

TEST_CASE("gesture_system: D key fires SwipeRight", "[gesture][desktop]") {
    auto reg = make_registry();
    reg.ctx().get<InputState>().key_d = true;

    gesture_system(reg, 0.016f);

    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::SwipeRight);
}

TEST_CASE("gesture_system: key_1 selects Circle shape", "[gesture][desktop]") {
    auto reg = make_registry();
    reg.ctx().get<InputState>().key_1 = true;

    gesture_system(reg, 0.016f);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    CHECK(btn.pressed);
    CHECK(btn.shape == Shape::Circle);
    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::None);
}

TEST_CASE("gesture_system: key_2 selects Triangle shape", "[gesture][desktop]") {
    auto reg = make_registry();
    reg.ctx().get<InputState>().key_2 = true;

    gesture_system(reg, 0.016f);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    CHECK(btn.pressed);
    CHECK(btn.shape == Shape::Triangle);
}

TEST_CASE("gesture_system: key_3 selects Square shape", "[gesture][desktop]") {
    auto reg = make_registry();
    reg.ctx().get<InputState>().key_3 = true;

    gesture_system(reg, 0.016f);

    auto& btn = reg.ctx().get<ShapeButtonEvent>();
    CHECK(btn.pressed);
    CHECK(btn.shape == Shape::Square);
}

TEST_CASE("gesture_system: keyboard key produces no magnitude or hit coords", "[gesture][desktop]") {
    auto reg = make_registry();
    reg.ctx().get<InputState>().key_w = true;

    gesture_system(reg, 0.016f);

    auto& g = reg.ctx().get<GestureResult>();
    CHECK(g.gesture == SwipeGesture::SwipeUp);
    CHECK(g.magnitude == 0.0f);
    CHECK(g.hit_x == 0.0f);
    CHECK(g.hit_y == 0.0f);
}

TEST_CASE("gesture_system: keyboard takes priority over simultaneous touch", "[gesture][desktop]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    // Valid right-swipe touch gesture
    input.touch_up = true;
    input.start_x  = 100.0f;
    input.start_y  = 400.0f;
    input.end_x    = 300.0f;
    input.end_y    = 400.0f;
    input.duration = 0.1f;
    // Simultaneously W was pressed
    input.key_w    = true;

    gesture_system(reg, 0.016f);

    // Keyboard wins
    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::SwipeUp);
}

TEST_CASE("gesture_system: key flags cleared between frames do not re-fire", "[gesture][desktop]") {
    auto reg = make_registry();
    auto& input = reg.ctx().get<InputState>();
    input.key_d = true;

    gesture_system(reg, 0.016f);
    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::SwipeRight);

    // Next frame: clear events (simulates start of input_system tick)
    clear_input_events(input);
    gesture_system(reg, 0.016f);
    CHECK(reg.ctx().get<GestureResult>().gesture == SwipeGesture::None);
}
#endif // PLATFORM_DESKTOP
