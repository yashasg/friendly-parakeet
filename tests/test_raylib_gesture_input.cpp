#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "input/raylib_gesture_input.h"
#include "test_helpers.h"
#include "constants.h"

TEST_CASE("raylib gesture input: bottom zone always Tap", "[input][gesture]") {
    float zone_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;

    auto evt = input_event_from_raylib_gesture(GESTURE_SWIPE_RIGHT, zone_y, 300.0f, zone_y);
    CHECK(evt.type == InputType::Tap);

    auto evt2 = input_event_from_raylib_gesture(GESTURE_SWIPE_UP, zone_y + 100.0f,
                                                300.0f, zone_y + 100.0f);
    CHECK(evt2.type == InputType::Tap);

    auto evt3 = input_event_from_raylib_gesture(GESTURE_NONE, zone_y + 50.0f,
                                                500.0f, zone_y + 100.0f);
    CHECK(evt3.type == InputType::Tap);
    CHECK_THAT(evt3.x, Catch::Matchers::WithinAbs(500.0f, 0.01f));
}

TEST_CASE("raylib gesture input: top zone routes raylib swipes", "[input][gesture]") {
    float zone_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;
    float start_y = zone_y - 10.0f;

    auto evt = input_event_from_raylib_gesture(GESTURE_SWIPE_RIGHT, start_y,
                                               200.0f, start_y);
    CHECK(evt.type == InputType::Swipe);
    CHECK(evt.dir == Direction::Right);

    auto evt2 = input_event_from_raylib_gesture(GESTURE_TAP, start_y,
                                                200.0f, start_y);
    CHECK(evt2.type == InputType::Tap);
}

TEST_CASE("raylib gesture input: end coordinates preserved", "[input][gesture]") {
    auto tap_evt = input_event_from_raylib_gesture(GESTURE_TAP, 100.0f, 150.0f, 250.0f);
    CHECK_THAT(tap_evt.x, Catch::Matchers::WithinAbs(150.0f, 0.01f));
    CHECK_THAT(tap_evt.y, Catch::Matchers::WithinAbs(250.0f, 0.01f));

    auto swipe_evt = input_event_from_raylib_gesture(GESTURE_SWIPE_RIGHT,
                                                     100.0f, 200.0f, 100.0f);
    CHECK_THAT(swipe_evt.x, Catch::Matchers::WithinAbs(200.0f, 0.01f));
    CHECK_THAT(swipe_evt.y, Catch::Matchers::WithinAbs(100.0f, 0.01f));
}

TEST_CASE("raylib gesture input: direction mapping", "[input][gesture]") {
    auto right = input_event_from_raylib_gesture(GESTURE_SWIPE_RIGHT, 100.0f, 200.0f, 100.0f);
    CHECK(right.type == InputType::Swipe);
    CHECK(right.dir == Direction::Right);

    auto left = input_event_from_raylib_gesture(GESTURE_SWIPE_LEFT, 100.0f, 100.0f, 100.0f);
    CHECK(left.type == InputType::Swipe);
    CHECK(left.dir == Direction::Left);

    auto down = input_event_from_raylib_gesture(GESTURE_SWIPE_DOWN, 100.0f, 100.0f, 200.0f);
    CHECK(down.type == InputType::Swipe);
    CHECK(down.dir == Direction::Down);

    auto up = input_event_from_raylib_gesture(GESTURE_SWIPE_UP, 100.0f, 100.0f, 100.0f);
    CHECK(up.type == InputType::Swipe);
    CHECK(up.dir == Direction::Up);
}

TEST_CASE("raylib gesture input: unknown gesture is tap", "[input][gesture]") {
    auto evt = input_event_from_raylib_gesture(GESTURE_NONE, 100.0f, 100.0f, 100.0f);
    CHECK(evt.type == InputType::Tap);
}

TEST_CASE("raylib gesture input: bottom-zone tap activates current button", "[input][gesture][hit_test]") {
    auto reg = make_registry();
    auto button = make_menu_button(reg, MenuActionKind::Confirm, GamePhase::Playing);
    reg.get<Position>(button) = {360.0f, 1100.0f};
    reg.get<HitBox>(button) = {80.0f, 40.0f};

    float zone_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;
    auto tap_event = input_event_from_raylib_gesture(GESTURE_SWIPE_RIGHT, zone_y + 5.0f,
                                                     360.0f, 1100.0f);
    push_input(reg, tap_event.type, tap_event.x, tap_event.y, tap_event.dir);
    run_input_tier1(reg);

    auto press_cap = drain_press_events(reg);
    REQUIRE(press_cap.count == 1);
    CHECK(press_cap.buf[0].kind        == ButtonPressKind::Menu);
    CHECK(press_cap.buf[0].menu_action == MenuActionKind::Confirm);

    auto swipe_event = input_event_from_raylib_gesture(GESTURE_SWIPE_RIGHT, zone_y - 5.0f,
                                                       360.0f, 1100.0f);
    push_input(reg, swipe_event.type, swipe_event.x, swipe_event.y, swipe_event.dir);
    run_input_tier1(reg);

    CHECK(drain_press_events(reg).count == 0);
    CHECK(drain_go_events(reg).count == 1);
}
