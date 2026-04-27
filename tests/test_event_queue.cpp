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

TEST_CASE("EventQueue: clear resets raw input count", "[events]") {
    EventQueue eq{};
    eq.push_input(InputType::Tap, 0.0f, 0.0f);
    CHECK(eq.input_count == 1);
    eq.clear();
    CHECK(eq.input_count == 0);
}

TEST_CASE("EventQueue: input overflow capped at MAX", "[events]") {
    EventQueue eq{};
    for (int i = 0; i < EventQueue::MAX + 5; ++i) {
        eq.push_input(InputType::Tap, 0.0f, 0.0f);
    }
    CHECK(eq.input_count == EventQueue::MAX);
}

TEST_CASE("dispatcher: GoEvent enqueue/update flows to listeners", "[events]") {
    entt::registry reg;
    reg.ctx().emplace<EventQueue>();
    reg.ctx().emplace<entt::dispatcher>();

    auto& disp = reg.ctx().get<entt::dispatcher>();
    GoCapture cap;
    disp.sink<GoEvent>().connect<&GoCapture::capture>(cap);

    disp.enqueue<GoEvent>({Direction::Right});
    disp.enqueue<GoEvent>({Direction::Up});
    disp.update<GoEvent>();
    disp.sink<GoEvent>().disconnect<&GoCapture::capture>(cap);

    REQUIRE(cap.count == 2);
    CHECK(cap.buf[0].dir == Direction::Right);
    CHECK(cap.buf[1].dir == Direction::Up);
}

TEST_CASE("dispatcher: ButtonPressEvent enqueue/update flows to listeners", "[events]") {
    entt::registry reg;
    reg.ctx().emplace<EventQueue>();
    reg.ctx().emplace<entt::dispatcher>();
    auto e1 = reg.create();
    auto e2 = reg.create();

    auto& disp = reg.ctx().get<entt::dispatcher>();
    PressCapture cap;
    disp.sink<ButtonPressEvent>().connect<&PressCapture::capture>(cap);

    disp.enqueue<ButtonPressEvent>({e1});
    disp.enqueue<ButtonPressEvent>({e2});
    disp.update<ButtonPressEvent>();
    disp.sink<ButtonPressEvent>().disconnect<&PressCapture::capture>(cap);

    REQUIRE(cap.count == 2);
    CHECK(cap.buf[0].entity == e1);
    CHECK(cap.buf[1].entity == e2);
}
