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

// ── Per-shape press event tests (#1202/#1204) ─────────────────────────────

TEST_CASE("dispatcher: ShapePress*Event enqueue/update", "[events]") {
    entt::registry reg;
    reg.ctx().emplace<entt::dispatcher>();

    auto& disp = reg.ctx().get<entt::dispatcher>();
    PressCapture cap;
    disp.sink<ShapePressCircleEvent>().connect<&PressCapture::on_circle>(cap);
    disp.sink<ShapePressSquareEvent>().connect<&PressCapture::on_square>(cap);
    disp.sink<ShapePressTriangleEvent>().connect<&PressCapture::on_triangle>(cap);
    disp.sink<MenuPressEvent>().connect<&PressCapture::on_menu>(cap);

    disp.enqueue<ShapePressSquareEvent>({});
    disp.enqueue<MenuPressEvent>({MenuActionKind::Restart, 0});
    disp.update<ShapePressCircleEvent>();
    disp.update<ShapePressSquareEvent>();
    disp.update<ShapePressTriangleEvent>();
    disp.update<MenuPressEvent>();
    disp.sink<ShapePressCircleEvent>().disconnect<&PressCapture::on_circle>(cap);
    disp.sink<ShapePressSquareEvent>().disconnect<&PressCapture::on_square>(cap);
    disp.sink<ShapePressTriangleEvent>().disconnect<&PressCapture::on_triangle>(cap);
    disp.sink<MenuPressEvent>().disconnect<&PressCapture::on_menu>(cap);

    REQUIRE(cap.shape_count() == 1);
    CHECK(cap.square == 1);
    CHECK(cap.circle == 0);
    CHECK(cap.triangle == 0);
    REQUIRE(cap.menu_count == 1);
    CHECK(cap.menu_buf[0].action == MenuActionKind::Restart);
}

TEST_CASE("dispatcher: press events are pure value types — no entity field", "[events]") {
    // Per #1202/#1204 (and the legacy #273 contract), no press event stores
    // an entity handle. Constructing an event remains valid regardless of
    // any entity lifecycle.
    entt::registry reg;
    auto btn_entity = reg.create();
    reg.destroy(btn_entity);  // entity is now invalid / recycled

    ShapePressTriangleEvent shape_evt{};
    MenuPressEvent menu_evt{MenuActionKind::Confirm, 3};
    (void)shape_evt;
    CHECK(menu_evt.action == MenuActionKind::Confirm);
    CHECK(menu_evt.index  == 3);
    (void)btn_entity;
}
