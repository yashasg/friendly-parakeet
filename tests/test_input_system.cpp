#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "systems/input_gesture.h"
#include "test_helpers.h"
#include "constants.h"

TEST_CASE("classify_touch_release: minimum swipe distance", "[input_gesture]") {
    auto evt = classify_touch_release(100.0f, 100.0f, 145.0f, 100.0f, 0.1f);
    CHECK(evt.type == InputType::Tap);
    CHECK_THAT(evt.x, Catch::Matchers::WithinAbs(145.0f, 0.01f));
    CHECK_THAT(evt.y, Catch::Matchers::WithinAbs(100.0f, 0.01f));

    auto evt2 = classify_touch_release(100.0f, 100.0f, 150.0f, 100.0f, 0.1f);
    CHECK(evt2.type == InputType::Swipe);
    CHECK(evt2.dir == Direction::Right);

    auto evt3 = classify_touch_release(100.0f, 100.0f, 151.0f, 100.0f, 0.1f);
    CHECK(evt3.type == InputType::Swipe);
    CHECK(evt3.dir == Direction::Right);
}

TEST_CASE("classify_touch_release: maximum swipe time", "[input_gesture]") {
    auto evt = classify_touch_release(100.0f, 100.0f, 200.0f, 100.0f,
                                       constants::MAX_SWIPE_TIME + 0.001f);
    CHECK(evt.type == InputType::Tap);

    auto evt2 = classify_touch_release(100.0f, 100.0f, 200.0f, 100.0f,
                                        constants::MAX_SWIPE_TIME);
    CHECK(evt2.type == InputType::Swipe);
    CHECK(evt2.dir == Direction::Right);

    auto evt3 = classify_touch_release(100.0f, 100.0f, 200.0f, 100.0f,
                                        constants::MAX_SWIPE_TIME - 0.001f);
    CHECK(evt3.type == InputType::Swipe);
}

TEST_CASE("classify_touch_release: bottom zone always Tap", "[input_gesture]") {
    float zone_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;

    auto evt = classify_touch_release(100.0f, zone_y, 300.0f, zone_y,
                                       constants::MAX_SWIPE_TIME);
    CHECK(evt.type == InputType::Tap);

    auto evt2 = classify_touch_release(100.0f, zone_y + 100.0f, 300.0f, zone_y + 100.0f,
                                        constants::MAX_SWIPE_TIME);
    CHECK(evt2.type == InputType::Tap);

    auto evt3 = classify_touch_release(100.0f, zone_y + 50.0f, 500.0f, zone_y + 100.0f,
                                        0.1f);
    CHECK(evt3.type == InputType::Tap);
    CHECK_THAT(evt3.x, Catch::Matchers::WithinAbs(500.0f, 0.01f));
}

TEST_CASE("classify_touch_release: top zone routes properly", "[input_gesture]") {
    float zone_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;
    float start_y = zone_y - 10.0f;

    auto evt = classify_touch_release(100.0f, start_y, 200.0f, start_y,
                                       0.1f);
    CHECK(evt.type == InputType::Swipe);
    CHECK(evt.dir == Direction::Right);

    auto evt2 = classify_touch_release(100.0f, start_y, 200.0f, start_y,
                                        constants::MAX_SWIPE_TIME + 0.01f);
    CHECK(evt2.type == InputType::Tap);
}

TEST_CASE("classify_touch_release: end coordinates preserved", "[input_gesture]") {
    auto tap_evt = classify_touch_release(100.0f, 100.0f, 150.0f, 250.0f, 0.5f);
    CHECK_THAT(tap_evt.x, Catch::Matchers::WithinAbs(150.0f, 0.01f));
    CHECK_THAT(tap_evt.y, Catch::Matchers::WithinAbs(250.0f, 0.01f));

    auto swipe_evt = classify_touch_release(100.0f, 100.0f, 200.0f, 100.0f, 0.1f);
    CHECK_THAT(swipe_evt.x, Catch::Matchers::WithinAbs(200.0f, 0.01f));
    CHECK_THAT(swipe_evt.y, Catch::Matchers::WithinAbs(100.0f, 0.01f));
}

TEST_CASE("classify_touch_release: direction detection", "[input_gesture]") {
    auto right = classify_touch_release(100.0f, 100.0f, 200.0f, 100.0f, 0.1f);
    CHECK(right.type == InputType::Swipe);
    CHECK(right.dir == Direction::Right);

    auto left = classify_touch_release(200.0f, 100.0f, 100.0f, 100.0f, 0.1f);
    CHECK(left.type == InputType::Swipe);
    CHECK(left.dir == Direction::Left);

    auto down = classify_touch_release(100.0f, 100.0f, 100.0f, 200.0f, 0.1f);
    CHECK(down.type == InputType::Swipe);
    CHECK(down.dir == Direction::Down);

    auto up = classify_touch_release(100.0f, 200.0f, 100.0f, 100.0f, 0.1f);
    CHECK(up.type == InputType::Swipe);
    CHECK(up.dir == Direction::Up);

    auto equal_down = classify_touch_release(100.0f, 100.0f, 150.0f, 150.0f, 0.1f);
    CHECK(equal_down.type == InputType::Swipe);
    CHECK(equal_down.dir == Direction::Down);

    auto equal_up = classify_touch_release(150.0f, 150.0f, 100.0f, 100.0f, 0.1f);
    CHECK(equal_up.type == InputType::Swipe);
    CHECK(equal_up.dir == Direction::Up);
}

TEST_CASE("classify_touch_release: diagonal swipes favor dominant axis", "[input_gesture]") {
    auto mostly_right = classify_touch_release(100.0f, 100.0f, 180.0f, 110.0f, 0.1f);
    CHECK(mostly_right.type == InputType::Swipe);
    CHECK(mostly_right.dir == Direction::Right);

    auto mostly_down = classify_touch_release(100.0f, 100.0f, 110.0f, 180.0f, 0.1f);
    CHECK(mostly_down.type == InputType::Swipe);
    CHECK(mostly_down.dir == Direction::Down);
}

TEST_CASE("classify_touch_release: boundary conditions", "[input_gesture]") {
    auto instant = classify_touch_release(100.0f, 100.0f, 200.0f, 100.0f, 0.0f);
    CHECK(instant.type == InputType::Swipe);

    auto tap_in_place = classify_touch_release(100.0f, 100.0f, 100.0f, 100.0f, 0.1f);
    CHECK(tap_in_place.type == InputType::Tap);

    auto boundary = classify_touch_release(100.0f, 100.0f,
                                            100.0f + constants::MIN_SWIPE_DIST, 100.0f,
                                            0.0f);
    CHECK(boundary.type == InputType::Swipe);
}

TEST_CASE("classify_touch_release: bottom-zone tap activates current button", "[input_gesture][hit_test]") {
    auto reg = make_registry();
    auto& eq = reg.ctx().get<EventQueue>();
    auto button = make_menu_button(reg, MenuActionKind::Confirm, GamePhase::Playing);
    reg.get<Position>(button) = {360.0f, 1100.0f};
    reg.get<HitBox>(button) = {80.0f, 40.0f};

    float zone_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;
    auto tap_event = classify_touch_release(50.0f, zone_y + 5.0f,
                                            360.0f, 1100.0f, 0.05f);
    eq.push_input(tap_event.type, tap_event.x, tap_event.y, tap_event.dir);
    hit_test_system(reg);

    auto press_cap = drain_press_events(reg);
    REQUIRE(press_cap.count == 1);
    CHECK(press_cap.buf[0].entity == button);

    eq.clear();
    auto swipe_event = classify_touch_release(50.0f, zone_y - 5.0f,
                                              360.0f, 1100.0f, 0.05f);
    eq.push_input(swipe_event.type, swipe_event.x, swipe_event.y, swipe_event.dir);
    gesture_routing_system(reg);
    hit_test_system(reg);

    CHECK(drain_press_events(reg).count == 0);
    CHECK(drain_go_events(reg).count == 1);
}
