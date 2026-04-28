#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// ── Raw InputEvent dispatcher tests (#333) ────────────────────────────────
// EventQueue has been removed.  Raw touch/mouse gestures are now enqueued as
// InputEvent objects into the entt::dispatcher and delivered to
// gesture_routing_handle_input + hit_test_handle_input via update<InputEvent>().

TEST_CASE("dispatcher: InputEvent enqueue/update fires listeners", "[events]") {
    entt::registry reg;
    reg.ctx().emplace<entt::dispatcher>();

    struct InputCapture {
        InputEvent buf[8]{};
        int        count{0};
        void capture(const InputEvent& e) { if (count < 8) buf[count++] = e; }
    } cap;

    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.sink<InputEvent>().connect<&InputCapture::capture>(cap);

    disp.enqueue<InputEvent>({InputType::Swipe, Direction::Left,  0.f, 0.f});
    disp.enqueue<InputEvent>({InputType::Tap,   Direction::Up,   100.f, 200.f});
    disp.update<InputEvent>();
    disp.sink<InputEvent>().disconnect<&InputCapture::capture>(cap);

    REQUIRE(cap.count == 2);
    CHECK(cap.buf[0].type == InputType::Swipe);
    CHECK(cap.buf[0].dir  == Direction::Left);
    CHECK(cap.buf[1].type == InputType::Tap);
    CHECK(cap.buf[1].x    == 100.f);
    CHECK(cap.buf[1].y    == 200.f);
}

TEST_CASE("dispatcher: InputEvent clear discards without firing listeners", "[events]") {
    entt::registry reg;
    reg.ctx().emplace<entt::dispatcher>();

    int fired = 0;
    struct Counter { int* count; void on_evt(const InputEvent&) { ++(*count); } } c{&fired};

    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.sink<InputEvent>().connect<&Counter::on_evt>(c);
    disp.enqueue<InputEvent>({InputType::Tap, Direction::Up, 0.f, 0.f});
    disp.clear<InputEvent>();  // R7: defensive discard, must not fire listener
    disp.sink<InputEvent>().disconnect<&Counter::on_evt>(c);

    CHECK(fired == 0);
}

// ── GoEvent dispatcher tests ──────────────────────────────────────────────

TEST_CASE("dispatcher: GoEvent enqueue/update flows to listeners", "[events]") {
    entt::registry reg;
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

// ── ButtonPressEvent semantic payload tests (#273) ────────────────────────

TEST_CASE("dispatcher: ButtonPressEvent semantic payload enqueue/update", "[events]") {
    entt::registry reg;
    reg.ctx().emplace<entt::dispatcher>();

    auto& disp = reg.ctx().get<entt::dispatcher>();
    PressCapture cap;
    disp.sink<ButtonPressEvent>().connect<&PressCapture::capture>(cap);

    disp.enqueue<ButtonPressEvent>({ButtonPressKind::Shape, Shape::Square,
                                   MenuActionKind::Confirm, 0});
    disp.enqueue<ButtonPressEvent>({ButtonPressKind::Menu, Shape::Circle,
                                   MenuActionKind::Restart, 0});
    disp.update<ButtonPressEvent>();
    disp.sink<ButtonPressEvent>().disconnect<&PressCapture::capture>(cap);

    REQUIRE(cap.count == 2);
    CHECK(cap.buf[0].kind  == ButtonPressKind::Shape);
    CHECK(cap.buf[0].shape == Shape::Square);
    CHECK(cap.buf[1].kind        == ButtonPressKind::Menu);
    CHECK(cap.buf[1].menu_action == MenuActionKind::Restart);
}

TEST_CASE("dispatcher: ButtonPressEvent stale-entity safety — no entity field", "[events]") {
    // #273: ButtonPressEvent no longer stores an entity handle.  Verify the
    // struct has no entity field and carries semantic data instead.
    entt::registry reg;
    auto btn_entity = reg.create();
    reg.destroy(btn_entity);  // entity is now invalid / recycled

    // Construct a ButtonPressEvent with semantic payload — previously this
    // would have stored the (now stale) entity handle.
    ButtonPressEvent evt{ButtonPressKind::Shape, Shape::Triangle,
                         MenuActionKind::Confirm, 0};
    // The event remains fully valid regardless of entity lifecycle.
    CHECK(evt.kind  == ButtonPressKind::Shape);
    CHECK(evt.shape == Shape::Triangle);
    (void)btn_entity;
}
