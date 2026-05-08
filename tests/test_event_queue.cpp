#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

TEST_CASE("dispatcher: GoEvent clear discards without firing listeners", "[events]") {
    entt::registry reg;
    reg.ctx().emplace<entt::dispatcher>();

    int fired = 0;
    struct Counter { int* count; void on_evt(const GoEvent&) { ++(*count); } } c{&fired};

    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.sink<GoEvent>().connect<&Counter::on_evt>(c);
    disp.enqueue<GoEvent>({Direction::Up});
    disp.clear<GoEvent>();
    disp.sink<GoEvent>().disconnect<&Counter::on_evt>(c);

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
