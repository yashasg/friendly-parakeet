#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "platform/input/sdl2_touch_tracker.h"

using Catch::Matchers::WithinAbs;

TEST_CASE("sdl2 touch tracker: tracks up to two simultaneous touches", "[input][touch][sdl2]") {
    platform::input::Sdl2TouchTracker tracker;
    tracker.configure_gameplay_gestures();

    tracker.on_finger_down(1, 10.0f, 20.0f, 10);
    tracker.on_finger_down(2, 30.0f, 40.0f, 12);
    tracker.on_finger_down(3, 50.0f, 60.0f, 14);  // ignored (max 2)

    CHECK(tracker.touch_point_count() == 2);

    float x = 0.0f;
    float y = 0.0f;
    REQUIRE(tracker.touch_position(0, x, y));
    CHECK_THAT(x, WithinAbs(10.0f, 0.01f));
    CHECK_THAT(y, WithinAbs(20.0f, 0.01f));
    REQUIRE(tracker.touch_position(1, x, y));
    CHECK_THAT(x, WithinAbs(30.0f, 0.01f));
    CHECK_THAT(y, WithinAbs(40.0f, 0.01f));
    CHECK_FALSE(tracker.touch_position(2, x, y));
}

TEST_CASE("sdl2 touch tracker: remaining touch compacts to index zero after release", "[input][touch][sdl2]") {
    platform::input::Sdl2TouchTracker tracker;
    tracker.configure_gameplay_gestures();

    tracker.on_finger_down(7, 100.0f, 120.0f, 100);
    tracker.on_finger_down(8, 200.0f, 220.0f, 110);
    tracker.on_finger_up(7, 102.0f, 121.0f, 130);

    CHECK(tracker.touch_point_count() == 1);
    float x = 0.0f;
    float y = 0.0f;
    REQUIRE(tracker.touch_position(0, x, y));
    CHECK_THAT(x, WithinAbs(200.0f, 0.01f));
    CHECK_THAT(y, WithinAbs(220.0f, 0.01f));
}

TEST_CASE("sdl2 touch tracker: maps swipes to gameplay gestures", "[input][gesture][sdl2]") {
    platform::input::Sdl2TouchTracker tracker;
    tracker.configure_gameplay_gestures();

    tracker.on_finger_down(1, 100.0f, 100.0f, 100);
    tracker.on_finger_up(1, 220.0f, 105.0f, 180);
    CHECK(tracker.consume_detected_gesture() == GESTURE_SWIPE_RIGHT);

    tracker.on_finger_down(2, 300.0f, 100.0f, 200);
    tracker.on_finger_up(2, 120.0f, 100.0f, 280);
    CHECK(tracker.consume_detected_gesture() == GESTURE_SWIPE_LEFT);

    tracker.on_finger_down(3, 100.0f, 300.0f, 300);
    tracker.on_finger_up(3, 100.0f, 120.0f, 360);
    CHECK(tracker.consume_detected_gesture() == GESTURE_SWIPE_UP);

    tracker.on_finger_down(4, 100.0f, 300.0f, 400);
    tracker.on_finger_up(4, 100.0f, 500.0f, 460);
    CHECK(tracker.consume_detected_gesture() == GESTURE_SWIPE_DOWN);
}

TEST_CASE("sdl2 touch tracker: non-swipe releases classify as tap", "[input][gesture][sdl2]") {
    platform::input::Sdl2TouchTracker tracker;
    tracker.configure_gameplay_gestures();

    tracker.on_finger_down(1, 50.0f, 50.0f, 100);
    tracker.on_finger_up(1, 70.0f, 60.0f, 140);  // below min distance
    CHECK(tracker.consume_detected_gesture() == GESTURE_TAP);

    tracker.on_finger_down(2, 50.0f, 50.0f, 200);
    tracker.on_finger_up(2, 180.0f, 50.0f, 650);  // exceeds max swipe duration
    CHECK(tracker.consume_detected_gesture() == GESTURE_TAP);
}

TEST_CASE("sdl2 touch tracker: gesture read is edge-triggered and carries timestamp", "[input][gesture][sdl2]") {
    platform::input::Sdl2TouchTracker tracker;
    tracker.configure_gameplay_gestures();

    tracker.on_finger_down(1, 0.0f, 0.0f, 1000);
    tracker.on_finger_up(1, 200.0f, 0.0f, 1080);

    CHECK(tracker.last_gesture_timestamp_ms() == 1080u);
    CHECK(tracker.consume_detected_gesture() == GESTURE_SWIPE_RIGHT);
    CHECK(tracker.consume_detected_gesture() == GESTURE_NONE);
}
