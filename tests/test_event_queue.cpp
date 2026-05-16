#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

TEST_CASE("dispatcher: GoEvent clear discards without firing listeners", "[events]") {
    entt::registry reg;
    reg.ctx().emplace<entt::dispatcher>();

    int fired = 0;
    struct Counter { int* count; void on_evt(const GoUpEvent&) { ++(*count); } } c{&fired};

    auto& disp = reg.ctx().get<entt::dispatcher>();
    disp.sink<GoUpEvent>().connect<&Counter::on_evt>(c);
    disp.enqueue<GoUpEvent>({});
    disp.clear<GoUpEvent>();
    disp.sink<GoUpEvent>().disconnect<&Counter::on_evt>(c);

    CHECK(fired == 0);
}

// ── Per-direction Go*Event dispatcher tests (#1279) ───────────────────────

TEST_CASE("dispatcher: Go*Event enqueue/update flows to listeners", "[events]") {
    entt::registry reg;
    reg.ctx().emplace<entt::dispatcher>();

    auto& disp = reg.ctx().get<entt::dispatcher>();
    GoCapture cap;
    disp.sink<GoUpEvent>()   .connect<&GoCapture::on_up>   (cap);
    disp.sink<GoRightEvent>().connect<&GoCapture::on_right>(cap);

    disp.enqueue<GoRightEvent>({});
    disp.enqueue<GoUpEvent>({});
    disp.update<GoUpEvent>();
    disp.update<GoRightEvent>();
    disp.sink<GoUpEvent>()   .disconnect<&GoCapture::on_up>   (cap);
    disp.sink<GoRightEvent>().disconnect<&GoCapture::on_right>(cap);

    REQUIRE(cap.count() == 2);
    CHECK(cap.right == 1);
    CHECK(cap.up    == 1);
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
    disp.sink<MenuRestartEvent>().connect<&PressCapture::on_restart>(cap);

    disp.enqueue<ShapePressSquareEvent>({});
    disp.enqueue<MenuRestartEvent>({});
    disp.update<ShapePressCircleEvent>();
    disp.update<ShapePressSquareEvent>();
    disp.update<ShapePressTriangleEvent>();
    disp.update<MenuRestartEvent>();
    disp.sink<ShapePressCircleEvent>().disconnect<&PressCapture::on_circle>(cap);
    disp.sink<ShapePressSquareEvent>().disconnect<&PressCapture::on_square>(cap);
    disp.sink<ShapePressTriangleEvent>().disconnect<&PressCapture::on_triangle>(cap);
    disp.sink<MenuRestartEvent>().disconnect<&PressCapture::on_restart>(cap);

    REQUIRE(cap.shape_count() == 1);
    CHECK(cap.square == 1);
    CHECK(cap.circle == 0);
    CHECK(cap.triangle == 0);
    REQUIRE(cap.menu_count() == 1);
    CHECK(cap.restart == 1);
}

TEST_CASE("dispatcher: press events are pure value types — no entity field", "[events]") {
    // Per #1202/#1204/#1277 (and the legacy #273 contract), no press event
    // stores an entity handle. Constructing an event remains valid regardless
    // of any entity lifecycle.
    entt::registry reg;
    auto btn_entity = reg.create();
    reg.destroy(btn_entity);  // entity is now invalid / recycled

    ShapePressTriangleEvent shape_evt{};
    MenuConfirmEvent        confirm_evt{};
    MenuSelectLevelEvent    select_evt{3};
    (void)shape_evt;
    (void)confirm_evt;
    CHECK(select_evt.index == 3);
    (void)btn_entity;
}
