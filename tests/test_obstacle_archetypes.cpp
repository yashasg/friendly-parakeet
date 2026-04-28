#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "entities/obstacle_entity.h"

// spawn_obstacle: entity bundle contract tests.
// Calls the entity factory directly, independent of beat_scheduler_system.

namespace {
struct ObstacleTagConstructProbe {
    bool saw_obstacle = false;
    bool saw_beat_info = false;
};

void probe_obstacle_tag_construct(entt::registry& reg, entt::entity entity) {
    auto* probe = reg.ctx().find<ObstacleTagConstructProbe>();
    if (!probe) return;
    probe->saw_obstacle = reg.all_of<Obstacle>(entity);
    probe->saw_beat_info = reg.all_of<BeatInfo>(entity);
}
}

TEST_CASE("entity: ShapeGate Circle - correct components and color", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::ShapeGate, 360.0f, -120.0f, Shape::Circle});

    REQUIRE(reg.all_of<ObstacleTag, Velocity, DrawLayer, Position, Obstacle, RequiredShape, DrawSize, Color>(e));
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

TEST_CASE("entity: ObstacleTag is emplaced after obstacle metadata", "[archetype]") {
    entt::registry reg;
    auto& probe = reg.ctx().emplace<ObstacleTagConstructProbe>();
    reg.on_construct<ObstacleTag>().connect<&probe_obstacle_tag_construct>();
    BeatInfo beat_info{7, 1.5f, 0.25f};

    spawn_obstacle(reg, {ObstacleKind::ShapeGate, 360.0f, -120.0f, Shape::Circle}, &beat_info);

    CHECK(probe.saw_obstacle);
    CHECK(probe.saw_beat_info);
}

TEST_CASE("entity: ShapeGate Square - red color", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::ShapeGate, 60.0f, -120.0f, Shape::Square});

    auto& c = reg.get<Color>(e);
    CHECK(c.r == 255); CHECK(c.g == 100); CHECK(c.b == 100);
}

TEST_CASE("entity: ShapeGate Triangle - green color", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::ShapeGate, 60.0f, -120.0f, Shape::Triangle});

    auto& c = reg.get<Color>(e);
    CHECK(c.r == 100); CHECK(c.g == 255); CHECK(c.b == 100);
}

TEST_CASE("entity: LaneBlock - blocked lanes and no shape components", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::LaneBlock, 60.0f, -120.0f,
                                   Shape::Circle, uint8_t{0b010}});

    REQUIRE(reg.all_of<ObstacleTag, Velocity, DrawLayer, Position, Obstacle, BlockedLanes, DrawSize, Color>(e));
    CHECK(!reg.all_of<RequiredShape>(e));
    CHECK(!reg.all_of<RequiredVAction>(e));
    CHECK(!reg.all_of<RequiredLane>(e));

    CHECK(reg.get<Obstacle>(e).kind == ObstacleKind::LaneBlock);
    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_LANE_BLOCK});
    CHECK(reg.get<BlockedLanes>(e).mask == uint8_t{0b010});
}

TEST_CASE("entity: LowBar - RequiredVAction Jumping", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::LowBar, 360.0f, -120.0f});

    REQUIRE(reg.all_of<ObstacleTag, Velocity, DrawLayer, ObstacleScrollZ, Obstacle, RequiredVAction, DrawSize, Color>(e));
    CHECK_FALSE(reg.all_of<Position>(e));
    CHECK(!reg.all_of<RequiredShape>(e));
    CHECK(!reg.all_of<BlockedLanes>(e));
    CHECK(!reg.all_of<RequiredLane>(e));

    CHECK(reg.get<Obstacle>(e).kind == ObstacleKind::LowBar);
    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_LOW_BAR});
    CHECK(reg.get<ObstacleScrollZ>(e).z == -120.0f);
    CHECK(reg.get<RequiredVAction>(e).action == VMode::Jumping);
    CHECK(reg.get<DrawSize>(e).h == 40.0f);
}

TEST_CASE("entity: HighBar - RequiredVAction Sliding", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::HighBar, 360.0f, -120.0f});

    REQUIRE(reg.all_of<ObstacleTag, Velocity, DrawLayer, ObstacleScrollZ, Obstacle, RequiredVAction, DrawSize, Color>(e));
    CHECK_FALSE(reg.all_of<Position>(e));
    CHECK(reg.get<Obstacle>(e).kind == ObstacleKind::HighBar);
    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_HIGH_BAR});
    CHECK(reg.get<ObstacleScrollZ>(e).z == -120.0f);
    CHECK(reg.get<RequiredVAction>(e).action == VMode::Sliding);
    CHECK(reg.get<DrawSize>(e).h == 40.0f);
}

TEST_CASE("entity: ComboGate - RequiredShape and BlockedLanes", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::ComboGate, 360.0f, -120.0f,
                                   Shape::Triangle, uint8_t{0b101}});

    REQUIRE(reg.all_of<ObstacleTag, Velocity, DrawLayer, Position, Obstacle, RequiredShape, BlockedLanes, DrawSize, Color>(e));
    CHECK(!reg.all_of<RequiredVAction>(e));
    CHECK(!reg.all_of<RequiredLane>(e));

    CHECK(reg.get<Obstacle>(e).kind == ObstacleKind::ComboGate);
    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_COMBO_GATE});
    CHECK(reg.get<RequiredShape>(e).shape == Shape::Triangle);
    CHECK(reg.get<BlockedLanes>(e).mask == uint8_t{0b101});
}

TEST_CASE("entity: SplitPath - RequiredShape and RequiredLane", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::SplitPath, 360.0f, -120.0f,
                                   Shape::Square, uint8_t{0}, int8_t{2}});

    REQUIRE(reg.all_of<ObstacleTag, Velocity, DrawLayer, Position, Obstacle, RequiredShape, RequiredLane, DrawSize, Color>(e));
    CHECK(!reg.all_of<RequiredVAction>(e));
    CHECK(!reg.all_of<BlockedLanes>(e));

    CHECK(reg.get<Obstacle>(e).kind == ObstacleKind::SplitPath);
    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_SPLIT_PATH});
    CHECK(reg.get<RequiredShape>(e).shape == Shape::Square);
    CHECK(reg.get<RequiredLane>(e).lane == int8_t{2});
}

TEST_CASE("entity: LanePushLeft - passive, no shape components", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::LanePushLeft, 60.0f, -120.0f});

    REQUIRE(reg.all_of<ObstacleTag, Velocity, DrawLayer, Position, Obstacle, DrawSize, Color>(e));
    CHECK(!reg.all_of<RequiredShape>(e));
    CHECK(!reg.all_of<RequiredVAction>(e));
    CHECK(!reg.all_of<BlockedLanes>(e));
    CHECK(!reg.all_of<RequiredLane>(e));

    CHECK(reg.get<Obstacle>(e).kind == ObstacleKind::LanePushLeft);
    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_LANE_PUSH});
}

TEST_CASE("entity: LanePushRight - passive, no shape components", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::LanePushRight, 660.0f, -120.0f});

    REQUIRE(reg.all_of<ObstacleTag, Velocity, DrawLayer, Position, Obstacle, DrawSize, Color>(e));
    CHECK(reg.get<Obstacle>(e).kind == ObstacleKind::LanePushRight);
    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_LANE_PUSH});
}

TEST_CASE("entity: LowBar ObstacleScrollZ z propagated from input", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::LowBar, 123.5f, 456.7f});

    REQUIRE(reg.all_of<ObstacleScrollZ>(e));
    CHECK_FALSE(reg.all_of<Position>(e));
    CHECK(reg.get<ObstacleScrollZ>(e).z == 456.7f);
}

TEST_CASE("entity: ShapeGate position x/y propagated from input", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::ShapeGate, 123.5f, 456.7f});

    auto& pos = reg.get<Position>(e);
    CHECK(pos.x == 123.5f);
    CHECK(pos.y == 456.7f);
}

TEST_CASE("entity: BeatInfo emplaced when provided", "[archetype]") {
    entt::registry reg;
    const BeatInfo bi{5, 1.5f, 1.0f};
    auto e = spawn_obstacle(reg, {ObstacleKind::ShapeGate, 360.0f, -120.0f}, &bi);

    REQUIRE(reg.all_of<BeatInfo>(e));
    CHECK(reg.get<BeatInfo>(e).beat_index == 5);
    CHECK(reg.get<BeatInfo>(e).arrival_time == 1.5f);
    CHECK(reg.get<BeatInfo>(e).spawn_time == 1.0f);
}

TEST_CASE("entity: no BeatInfo when not provided", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::ShapeGate, 360.0f, -120.0f});

    CHECK_FALSE(reg.all_of<BeatInfo>(e));
}
