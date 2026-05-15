#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "entities/obstacle_entity.h"
#include "entities/obstacle_render_entity.h"

#include <stdexcept>

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

int count_mesh_children(entt::registry& reg) {
    int count = 0;
    auto view = reg.view<MeshChild>();
    for ([[maybe_unused]] auto entity : view) {
        ++count;
    }
    return count;
}

entt::entity make_mesh_factory_obstacle(entt::registry& reg) {
    auto parent = reg.create();
    reg.emplace<WorldTransform>(parent, WorldTransform{{constants::LANE_X[1], -120.0f}});
    reg.emplace<Obstacle>(parent, int16_t{0});
    reg.emplace<DrawSize>(parent, constants::SCREEN_W_F, 80.0f);
    reg.emplace<Color>(parent, Color{255, 255, 255, 255});
    return parent;
}

void check_no_mesh_children(entt::registry& reg, entt::entity parent) {
    CHECK(count_mesh_children(reg) == 0);
    if (auto* children = reg.try_get<ObstacleChildren>(parent)) {
        CHECK(children->count == 0);
    }
}
}

TEST_CASE("entity: ShapeGate Circle - correct components and color", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::ShapeGate, 360.0f, -120.0f, Shape::Circle});

    REQUIRE(reg.all_of<ObstacleTag, MotionVelocity, DrawLayer, WorldTransform, Obstacle, RequiredShape, ShapeGateLane, DrawSize, Color>(e));
    CHECK(!reg.all_of<uint8_t>(e));
    CHECK(!reg.all_of<RequiredLane>(e));

    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_SHAPE_GATE});
    CHECK(reg.get<RequiredShape>(e).shape == Shape::Circle);
    CHECK(reg.get<ShapeGateLane>(e).lane == int8_t{1});
    CHECK(reg.get<WorldTransform>(e).position.x == 360.0f);
    CHECK(reg.get<WorldTransform>(e).position.y == -120.0f);

    auto& c = reg.get<Color>(e);
    CHECK(c.r == 80); CHECK(c.g == 200); CHECK(c.b == 255);
}

TEST_CASE("entity: ObstacleTag is emplaced after obstacle metadata", "[archetype]") {
    entt::registry reg;
    auto& probe = reg.ctx().emplace<ObstacleTagConstructProbe>();
    reg.on_construct<ObstacleTag>().connect<&probe_obstacle_tag_construct>();
    BeatInfo beat_info{7, 1.5f, 0.25f};

    spawn_rhythm_obstacle(reg, {ObstacleKind::ShapeGate, 360.0f, -120.0f, Shape::Circle}, beat_info);

    CHECK(probe.saw_obstacle);
    CHECK(probe.saw_beat_info);
}

TEST_CASE("entity: obstacle roots and mesh children declare world render pass", "[archetype][render]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::ShapeGate, 360.0f, -120.0f, Shape::Circle});

    CHECK(reg.all_of<TagWorldPass>(e));
    REQUIRE(reg.all_of<ObstacleChildren>(e));
    const auto& children = reg.get<ObstacleChildren>(e);
    REQUIRE(children.count > 0);
    for (int i = 0; i < children.count; ++i) {
        CHECK(reg.all_of<MeshChild>(children.children[i]));
        CHECK(reg.all_of<TagWorldPass>(children.children[i]));
    }
}

TEST_CASE("entity: obstacle mesh overflow does not create orphan MeshChild", "[archetype][render][cleanup]") {
    entt::registry reg;

    auto parent = reg.create();
    reg.emplace<WorldTransform>(parent, WorldTransform{{360.0f, -120.0f}});
    reg.emplace<Obstacle>(parent, int16_t{constants::PTS_SHAPE_GATE});
    reg.emplace<DrawSize>(parent, constants::SCREEN_W_F, 80.0f);
    reg.emplace<Color>(parent, Color{80, 200, 255, 255});
    reg.emplace<RequiredShape>(parent, Shape::Circle);

    auto& children = reg.emplace<ObstacleChildren>(parent);
    for (int i = 0; i < ObstacleChildren::MAX; ++i) {
        auto child = reg.create();
        reg.emplace<MeshChild>(child, MeshChild{
            parent, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
            Color{255, 255, 255, 255}, 0, MeshType::Slab
        });
        children.children[children.count++] = child;
    }
    reg.emplace<ObstacleTag>(parent);

    CHECK(count_mesh_children(reg) == ObstacleChildren::MAX);
    CHECK_THROWS_AS(spawn_obstacle_meshes(reg, parent), std::logic_error);
    CHECK(count_mesh_children(reg) == ObstacleChildren::MAX);
    CHECK(reg.get<ObstacleChildren>(parent).count == ObstacleChildren::MAX);

    destroy_obstacle_with_children(reg, parent);

    CHECK(count_mesh_children(reg) == 0);
}

TEST_CASE("entity: obstacle mesh lifetime helper cleans factory children", "[archetype][render][cleanup]") {
    entt::registry reg;
    auto parent = spawn_obstacle(reg, {ObstacleKind::ShapeGate, 360.0f, -120.0f, Shape::Circle});

    REQUIRE(count_mesh_children(reg) > 0);

    destroy_obstacle_with_children(reg, parent);

    CHECK(count_mesh_children(reg) == 0);
}

TEST_CASE("entity: direct mesh factory cleanup does not depend on ObstacleTag order", "[archetype][render][cleanup]") {
    entt::registry reg;
    auto parent = make_mesh_factory_obstacle(reg);
    reg.emplace<RequiredShape>(parent, Shape::Circle);
    reg.emplace<ObstacleTag>(parent);

    spawn_obstacle_meshes(reg, parent);
    REQUIRE(count_mesh_children(reg) > 0);

    destroy_obstacle_with_children(reg, parent);

    CHECK(count_mesh_children(reg) == 0);
}

TEST_CASE("entity: mesh factory rejects invalid RequiredShape before children", "[archetype][render][validation]") {
    const Shape invalid_shape = static_cast<Shape>(255);

    SECTION("ShapeGate") {
        entt::registry reg;
        auto parent = make_mesh_factory_obstacle(reg);
        reg.emplace<RequiredShape>(parent, invalid_shape);

        CHECK_THROWS_AS(spawn_obstacle_meshes(reg, parent), std::logic_error);
        check_no_mesh_children(reg, parent);
    }

    SECTION("ComboGate") {
        entt::registry reg;
        auto parent = make_mesh_factory_obstacle(reg);
        reg.emplace<RequiredShape>(parent, invalid_shape);
        reg.emplace<uint8_t>(parent, uint8_t{0b101});

        CHECK_THROWS_AS(spawn_obstacle_meshes(reg, parent), std::logic_error);
        check_no_mesh_children(reg, parent);
    }

    SECTION("SplitPath") {
        entt::registry reg;
        auto parent = make_mesh_factory_obstacle(reg);
        reg.emplace<RequiredShape>(parent, invalid_shape);
        reg.emplace<RequiredLane>(parent, int8_t{1});

        CHECK_THROWS_AS(spawn_obstacle_meshes(reg, parent), std::logic_error);
        check_no_mesh_children(reg, parent);
    }
}

TEST_CASE("entity: mesh factory rejects invalid RequiredLane before children", "[archetype][render][validation]") {
    SECTION("negative lane") {
        entt::registry reg;
        auto parent = make_mesh_factory_obstacle(reg);
        reg.emplace<RequiredShape>(parent, Shape::Square);
        reg.emplace<RequiredLane>(parent, int8_t{-1});

        CHECK_THROWS_AS(spawn_obstacle_meshes(reg, parent), std::logic_error);
        check_no_mesh_children(reg, parent);
    }

    SECTION("lane beyond lane table") {
        entt::registry reg;
        auto parent = make_mesh_factory_obstacle(reg);
        reg.emplace<RequiredShape>(parent, Shape::Square);
        reg.emplace<RequiredLane>(parent, int8_t{3});

        CHECK_THROWS_AS(spawn_obstacle_meshes(reg, parent), std::logic_error);
        check_no_mesh_children(reg, parent);
    }
}

TEST_CASE("entity: spawn_obstacle rejects invalid ShapeGate shape before indexing color",
          "[archetype][validation]") {
    entt::registry reg;
    const Shape invalid_shape = static_cast<Shape>(255);

    CHECK_THROWS_AS(spawn_obstacle(reg, {ObstacleKind::ShapeGate, 360.0f, -120.0f, invalid_shape}),
                    std::logic_error);
    CHECK(count_mesh_children(reg) == 0);
}

TEST_CASE("entity: spawn_obstacle rejects invalid SplitPath lane before creating entities",
          "[archetype][validation][issue1046]") {
    entt::registry reg;

    CHECK_THROWS_AS(spawn_obstacle(reg, {ObstacleKind::SplitPath, 360.0f, -120.0f,
                                         Shape::Square, uint8_t{0}, int8_t{3}}),
                    std::logic_error);
    CHECK(reg.view<ObstacleTag>().begin() == reg.view<ObstacleTag>().end());
    CHECK(reg.view<WorldTransform>().begin() == reg.view<WorldTransform>().end());
    CHECK(count_mesh_children(reg) == 0);
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

TEST_CASE("entity: deprecated lane obstacles are rejected by runtime factory",
          "[archetype][legacy][issue1027]") {
    entt::registry reg;

    CHECK_THROWS_AS(spawn_obstacle(reg, {ObstacleKind::LaneBlock, 60.0f, -120.0f,
                                         Shape::Circle, uint8_t{0b010}}),
                    std::logic_error);
    CHECK_THROWS_AS(spawn_obstacle(reg, {ObstacleKind::ComboGate, 360.0f, -120.0f,
                                         Shape::Triangle, uint8_t{0b101}}),
                    std::logic_error);
    CHECK(reg.view<ObstacleTag>().begin() == reg.view<ObstacleTag>().end());
}

TEST_CASE("entity: SplitPath - RequiredShape and RequiredLane", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::SplitPath, 360.0f, -120.0f,
                                   Shape::Square, uint8_t{0}, int8_t{2}});

    REQUIRE(reg.all_of<ObstacleTag, MotionVelocity, DrawLayer, WorldTransform, Obstacle, RequiredShape, RequiredLane, DrawSize, Color>(e));
    CHECK(!reg.all_of<uint8_t>(e));

    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_SPLIT_PATH});
    CHECK(reg.get<RequiredShape>(e).shape == Shape::Square);
    CHECK(reg.get<RequiredLane>(e).lane == int8_t{2});
}


TEST_CASE("entity: ShapeGate position x/y propagated from input", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::ShapeGate, 123.5f, 456.7f});

    auto& transform = reg.get<WorldTransform>(e);
    CHECK(transform.position.x == 123.5f);
    CHECK(transform.position.y == 456.7f);
}

TEST_CASE("entity: BeatInfo emplaced when provided", "[archetype]") {
    entt::registry reg;
    const BeatInfo bi{5, 1.5f, 1.0f};
    auto e = spawn_rhythm_obstacle(reg, {ObstacleKind::ShapeGate, 360.0f, -120.0f}, bi);

    REQUIRE(reg.all_of<BeatInfo>(e));
    CHECK_FALSE(reg.all_of<MotionVelocity>(e));
    CHECK(reg.get<BeatInfo>(e).beat_index == 5);
    CHECK(reg.get<BeatInfo>(e).arrival_time == 1.5f);
    CHECK(reg.get<BeatInfo>(e).spawn_time == 1.0f);
}

TEST_CASE("entity: no BeatInfo when not provided", "[archetype]") {
    entt::registry reg;
    auto e = spawn_obstacle(reg, {ObstacleKind::ShapeGate, 360.0f, -120.0f});

    CHECK_FALSE(reg.all_of<BeatInfo>(e));
    CHECK(reg.all_of<MotionVelocity>(e));
}
