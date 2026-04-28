#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "systems/obstacle_archetypes.h"

// ── apply_obstacle_archetype: direct factory contract tests ──
//
// These tests call the shared factory directly, independent of either
// beat_scheduler_system or obstacle_spawn_system.  They lock in the
// component set for each kind so divergence is caught immediately.

static entt::entity make_bare_entity(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);
    return e;
}

TEST_CASE("archetype: ShapeGate Circle — correct components and color", "[archetype]") {
    entt::registry reg;
    auto e = make_bare_entity(reg);
    apply_obstacle_archetype(reg, e, {ObstacleKind::ShapeGate, 360.0f, -120.0f, Shape::Circle});

    REQUIRE(reg.all_of<Position, Obstacle, RequiredShape, DrawSize, Color>(e));
    CHECK(!reg.all_of<RequiredVAction>(e));
    CHECK(!reg.all_of<BlockedLanes>(e));
    CHECK(!reg.all_of<RequiredLane>(e));

    CHECK(reg.get<Obstacle>(e).kind == ObstacleKind::ShapeGate);
    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_SHAPE_GATE});
    CHECK(reg.get<RequiredShape>(e).shape == Shape::Circle);
    CHECK(reg.get<Position>(e).x == 360.0f);
    CHECK(reg.get<Position>(e).y == -120.0f);

    auto& c = reg.get<Color>(e);
    CHECK(c.r == 80); CHECK(c.g == 200); CHECK(c.b == 255);
}

TEST_CASE("archetype: ShapeGate Square — red color", "[archetype]") {
    entt::registry reg;
    auto e = make_bare_entity(reg);
    apply_obstacle_archetype(reg, e, {ObstacleKind::ShapeGate, 60.0f, -120.0f, Shape::Square});

    auto& c = reg.get<Color>(e);
    CHECK(c.r == 255); CHECK(c.g == 100); CHECK(c.b == 100);
}

TEST_CASE("archetype: ShapeGate Triangle — green color", "[archetype]") {
    entt::registry reg;
    auto e = make_bare_entity(reg);
    apply_obstacle_archetype(reg, e, {ObstacleKind::ShapeGate, 60.0f, -120.0f, Shape::Triangle});

    auto& c = reg.get<Color>(e);
    CHECK(c.r == 100); CHECK(c.g == 255); CHECK(c.b == 100);
}

TEST_CASE("archetype: LaneBlock — blocked lanes and no shape components", "[archetype]") {
    entt::registry reg;
    auto e = make_bare_entity(reg);
    apply_obstacle_archetype(reg, e, {ObstacleKind::LaneBlock, 60.0f, -120.0f,
                                      Shape::Circle, uint8_t{0b010}});

    REQUIRE(reg.all_of<Position, Obstacle, BlockedLanes, DrawSize, Color>(e));
    CHECK(!reg.all_of<RequiredShape>(e));
    CHECK(!reg.all_of<RequiredVAction>(e));
    CHECK(!reg.all_of<RequiredLane>(e));

    CHECK(reg.get<Obstacle>(e).kind == ObstacleKind::LaneBlock);
    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_LANE_BLOCK});
    CHECK(reg.get<BlockedLanes>(e).mask == uint8_t{0b010});
}

TEST_CASE("archetype: LowBar — RequiredVAction Jumping", "[archetype]") {
    entt::registry reg;
    auto e = make_bare_entity(reg);
    apply_obstacle_archetype(reg, e, {ObstacleKind::LowBar, 360.0f, -120.0f});

    REQUIRE(reg.all_of<Position, Obstacle, RequiredVAction, DrawSize, Color>(e));
    CHECK(!reg.all_of<RequiredShape>(e));
    CHECK(!reg.all_of<BlockedLanes>(e));
    CHECK(!reg.all_of<RequiredLane>(e));

    CHECK(reg.get<Obstacle>(e).kind == ObstacleKind::LowBar);
    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_LOW_BAR});
    CHECK(reg.get<RequiredVAction>(e).action == VMode::Jumping);
    CHECK(reg.get<DrawSize>(e).h == 40.0f);
}

TEST_CASE("archetype: HighBar — RequiredVAction Sliding", "[archetype]") {
    entt::registry reg;
    auto e = make_bare_entity(reg);
    apply_obstacle_archetype(reg, e, {ObstacleKind::HighBar, 360.0f, -120.0f});

    REQUIRE(reg.all_of<RequiredVAction>(e));
    CHECK(reg.get<Obstacle>(e).kind == ObstacleKind::HighBar);
    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_HIGH_BAR});
    CHECK(reg.get<RequiredVAction>(e).action == VMode::Sliding);
    CHECK(reg.get<DrawSize>(e).h == 40.0f);
}

TEST_CASE("archetype: ComboGate — RequiredShape and BlockedLanes", "[archetype]") {
    entt::registry reg;
    auto e = make_bare_entity(reg);
    apply_obstacle_archetype(reg, e, {ObstacleKind::ComboGate, 360.0f, -120.0f,
                                      Shape::Triangle, uint8_t{0b101}});

    REQUIRE(reg.all_of<Position, Obstacle, RequiredShape, BlockedLanes, DrawSize, Color>(e));
    CHECK(!reg.all_of<RequiredVAction>(e));
    CHECK(!reg.all_of<RequiredLane>(e));

    CHECK(reg.get<Obstacle>(e).kind == ObstacleKind::ComboGate);
    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_COMBO_GATE});
    CHECK(reg.get<RequiredShape>(e).shape == Shape::Triangle);
    CHECK(reg.get<BlockedLanes>(e).mask == uint8_t{0b101});
}

TEST_CASE("archetype: SplitPath — RequiredShape and RequiredLane", "[archetype]") {
    entt::registry reg;
    auto e = make_bare_entity(reg);
    apply_obstacle_archetype(reg, e, {ObstacleKind::SplitPath, 360.0f, -120.0f,
                                      Shape::Square, uint8_t{0}, int8_t{2}});

    REQUIRE(reg.all_of<Position, Obstacle, RequiredShape, RequiredLane, DrawSize, Color>(e));
    CHECK(!reg.all_of<RequiredVAction>(e));
    CHECK(!reg.all_of<BlockedLanes>(e));

    CHECK(reg.get<Obstacle>(e).kind == ObstacleKind::SplitPath);
    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_SPLIT_PATH});
    CHECK(reg.get<RequiredShape>(e).shape == Shape::Square);
    CHECK(reg.get<RequiredLane>(e).lane == int8_t{2});
}

TEST_CASE("archetype: LanePushLeft — passive, no shape components", "[archetype]") {
    entt::registry reg;
    auto e = make_bare_entity(reg);
    apply_obstacle_archetype(reg, e, {ObstacleKind::LanePushLeft, 60.0f, -120.0f});

    REQUIRE(reg.all_of<Position, Obstacle, DrawSize, Color>(e));
    CHECK(!reg.all_of<RequiredShape>(e));
    CHECK(!reg.all_of<RequiredVAction>(e));
    CHECK(!reg.all_of<BlockedLanes>(e));
    CHECK(!reg.all_of<RequiredLane>(e));

    CHECK(reg.get<Obstacle>(e).kind == ObstacleKind::LanePushLeft);
    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_LANE_PUSH});
}

TEST_CASE("archetype: LanePushRight — passive, no shape components", "[archetype]") {
    entt::registry reg;
    auto e = make_bare_entity(reg);
    apply_obstacle_archetype(reg, e, {ObstacleKind::LanePushRight, 660.0f, -120.0f});

    REQUIRE(reg.all_of<Position, Obstacle, DrawSize, Color>(e));
    CHECK(reg.get<Obstacle>(e).kind == ObstacleKind::LanePushRight);
    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_LANE_PUSH});
}

TEST_CASE("archetype: position x/y propagated from input", "[archetype]") {
    entt::registry reg;
    auto e = make_bare_entity(reg);
    apply_obstacle_archetype(reg, e, {ObstacleKind::LowBar, 123.5f, 456.7f});

    auto& pos = reg.get<Position>(e);
    CHECK(pos.x == 123.5f);
    CHECK(pos.y == 456.7f);
}
