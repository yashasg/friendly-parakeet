#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// ── EventQueue unit tests ──────────────────────────────────────────

TEST_CASE("EventQueue: push_input stores tap", "[events]") {
    EventQueue eq{};
    eq.push_input(InputType::Tap, 100.0f, 200.0f);
    CHECK(eq.input_count == 1);
    CHECK(eq.inputs[0].type == InputType::Tap);
    CHECK(eq.inputs[0].x == 100.0f);
    CHECK(eq.inputs[0].y == 200.0f);
}

TEST_CASE("EventQueue: push_input stores swipe with direction", "[events]") {
    EventQueue eq{};
    eq.push_input(InputType::Swipe, 50.0f, 60.0f, Direction::Left);
    CHECK(eq.input_count == 1);
    CHECK(eq.inputs[0].type == InputType::Swipe);
    CHECK(eq.inputs[0].dir == Direction::Left);
    CHECK(eq.inputs[0].x == 50.0f);
    CHECK(eq.inputs[0].y == 60.0f);
}

TEST_CASE("EventQueue: push_go stores direction", "[events]") {
    EventQueue eq{};
    eq.push_go(Direction::Right);
    eq.push_go(Direction::Up);
    CHECK(eq.go_count == 2);
    CHECK(eq.goes[0].dir == Direction::Right);
    CHECK(eq.goes[1].dir == Direction::Up);
}

TEST_CASE("EventQueue: push_press stores entity", "[events]") {
    entt::registry reg;
    auto e1 = reg.create();
    auto e2 = reg.create();
    EventQueue eq{};
    eq.push_press(e1);
    eq.push_press(e2);
    CHECK(eq.press_count == 2);
    CHECK(eq.presses[0].entity == e1);
    CHECK(eq.presses[1].entity == e2);
}

TEST_CASE("EventQueue: clear resets all counts", "[events]") {
    EventQueue eq{};
    eq.push_input(InputType::Tap, 0.0f, 0.0f);
    eq.push_go(Direction::Left);
    eq.push_press(entt::null);
    CHECK(eq.input_count == 1);
    CHECK(eq.go_count == 1);
    CHECK(eq.press_count == 1);
    eq.clear();
    CHECK(eq.input_count == 0);
    CHECK(eq.go_count == 0);
    CHECK(eq.press_count == 0);
}

TEST_CASE("EventQueue: overflow capped at MAX", "[events]") {
    EventQueue eq{};
    for (int i = 0; i < EventQueue::MAX + 5; ++i) {
        eq.push_input(InputType::Tap, 0.0f, 0.0f);
        eq.push_go(Direction::Left);
        eq.push_press(entt::null);
    }
    CHECK(eq.input_count == EventQueue::MAX);
    CHECK(eq.go_count == EventQueue::MAX);
    CHECK(eq.press_count == EventQueue::MAX);
}
