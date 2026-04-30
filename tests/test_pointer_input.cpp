#include <catch2/catch_test_macros.hpp>
#include "input/pointer_input.h"

TEST_CASE("pointer input: release requires touch_up edge", "[input][pointer]") {
    InputState input{};
    input.touch_up = false;
    input.end_x = 100.0f;
    input.end_y = 200.0f;

    Vector2 pos{};
    CHECK_FALSE(pointer_release_position(input, pos));
}

TEST_CASE("pointer input: mouse-down alone is not treated as release", "[input][pointer]") {
    InputState input{};
    input.touch_down = true;
    input.active_source = InputSource::Mouse;
    input.start_x = 50.0f;
    input.start_y = 75.0f;

    Vector2 pos{};
    CHECK_FALSE(pointer_release_position(input, pos));
}

TEST_CASE("pointer input: release returns normalized coordinates", "[input][pointer]") {
    InputState input{};
    input.touch_up = true;
    input.end_x = 321.5f;
    input.end_y = 987.25f;

    Vector2 pos{};
    REQUIRE(pointer_release_position(input, pos));
    CHECK(pos.x == input.end_x);
    CHECK(pos.y == input.end_y);
}

TEST_CASE("pointer input: activate returns release coordinates", "[input][pointer]") {
    InputState input{};
    input.touch_up = true;
    input.end_x = 123.0f;
    input.end_y = 456.0f;

    Vector2 pos{};
    REQUIRE(pointer_activate_position(input, pos));
    CHECK(pos.x == input.end_x);
    CHECK(pos.y == input.end_y);
}
