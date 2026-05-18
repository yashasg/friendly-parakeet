#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"
#include "entities/obstacle_entity.h"
#include "entities/obstacle_render_entity.h"
#include "tags/tags.h"

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
    reg.emplace<WorldPosition>(parent, WorldPosition{{constants::LANE_X[1], -120.0f}});
    reg.emplace<Obstacle>(parent, int16_t{0});
    reg.emplace<DrawSize>(parent, constants::SCREEN_W_F, 80.0f);
    reg.emplace<Color>(parent, Color{255, 255, 255, 255});
    return parent;
}

void check_no_mesh_children(entt::registry& reg, entt::entity parent) {
    (void)parent;
    CHECK(count_mesh_children(reg) == 0);
}
}

TEST_CASE("entity: ShapeGate Circle - correct components and color", "[archetype]") {
    entt::registry reg;
    auto e = spawn_shape_gate_obstacle(reg, {360.0f, -120.0f, Shape::Circle});

    REQUIRE(reg.all_of<ObstacleTag, Vector2, WorldPosition, Obstacle, int8_t, DrawSize, Color>(e));
    REQUIRE(has_required_shape_tag(reg, e));
    CHECK(!reg.all_of<uint8_t>(e));
    CHECK(!reg.all_of<SplitPathTag>(e));

    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_SHAPE_GATE});
    CHECK(current_required_shape(reg, e) == Shape::Circle);
    CHECK(reg.get<int8_t>(e) == int8_t{1});
    CHECK(reg.get<WorldPosition>(e).position.x == 360.0f);
    CHECK(reg.get<WorldPosition>(e).position.y == -120.0f);

    auto& c = reg.get<Color>(e);
    CHECK(c.r == 80); CHECK(c.g == 200); CHECK(c.b == 255);
}

TEST_CASE("entity: ObstacleTag is emplaced after obstacle metadata", "[archetype]") {
    entt::registry reg;
    auto& probe = reg.ctx().emplace<ObstacleTagConstructProbe>();
    reg.on_construct<ObstacleTag>().connect<&probe_obstacle_tag_construct>();
    BeatInfo beat_info{7, 1.5f, 0.25f};

    spawn_shape_gate_rhythm(reg, {360.0f, -120.0f, Shape::Circle}, beat_info);

    CHECK(probe.saw_obstacle);
    CHECK(probe.saw_beat_info);
}

TEST_CASE("entity: obstacle roots and mesh children declare world render pass", "[archetype][render]") {
    entt::registry reg;
    auto e = spawn_shape_gate_obstacle(reg, {360.0f, -120.0f, Shape::Circle});

    CHECK(reg.all_of<TagWorldPass>(e));
    int child_count = 0;
    for (auto [child, mc] : reg.view<MeshChild>().each()) {
        if (mc.parent != e) continue;
        ++child_count;
        CHECK(reg.all_of<MeshChild>(child));
        CHECK(reg.all_of<TagWorldPass>(child));
    }
    REQUIRE(child_count > 0);
}

TEST_CASE("entity: obstacle mesh overflow does not create orphan MeshChild", "[archetype][render][cleanup]") {
    entt::registry reg;

    auto parent = reg.create();
    reg.emplace<WorldPosition>(parent, WorldPosition{{360.0f, -120.0f}});
    reg.emplace<Obstacle>(parent, int16_t{constants::PTS_SHAPE_GATE});
    reg.emplace<DrawSize>(parent, constants::SCREEN_W_F, 80.0f);
    reg.emplace<Color>(parent, Color{80, 200, 255, 255});
    set_required_shape_tag(reg, parent, Shape::Circle);

    for (int i = 0; i < ObstacleChildren::MAX; ++i) {
        auto child = reg.create();
        reg.emplace<MeshChild>(child, MeshChild{
            parent, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
            Color{255, 255, 255, 255}
        });
        reg.emplace<MeshKindSlab>(child);
    }
    reg.emplace<ObstacleTag>(parent);
    reg.emplace<ShapeGateTag>(parent);

    CHECK(count_mesh_children(reg) == ObstacleChildren::MAX);
    CHECK_THROWS_AS(spawn_obstacle_meshes(reg, parent), std::logic_error);
    CHECK(count_mesh_children(reg) == ObstacleChildren::MAX);

    destroy_obstacle_with_children(reg, parent);

    CHECK(count_mesh_children(reg) == 0);
}

TEST_CASE("entity: obstacle mesh lifetime helper cleans factory children", "[archetype][render][cleanup]") {
    entt::registry reg;
    auto parent = spawn_shape_gate_obstacle(reg, {360.0f, -120.0f, Shape::Circle});

    REQUIRE(count_mesh_children(reg) > 0);

    destroy_obstacle_with_children(reg, parent);

    CHECK(count_mesh_children(reg) == 0);
}

TEST_CASE("entity: direct mesh factory cleanup does not depend on ObstacleTag order", "[archetype][render][cleanup]") {
    entt::registry reg;
    auto parent = make_mesh_factory_obstacle(reg);
    set_required_shape_tag(reg, parent, Shape::Circle);
    reg.emplace<ObstacleTag>(parent);
    reg.emplace<ShapeGateTag>(parent);

    spawn_obstacle_meshes(reg, parent);
    REQUIRE(count_mesh_children(reg) > 0);

    destroy_obstacle_with_children(reg, parent);

    CHECK(count_mesh_children(reg) == 0);
}

TEST_CASE("entity: mesh factory rejects invalid required lane before children", "[archetype][render][validation]") {
    SECTION("negative lane") {
        entt::registry reg;
        auto parent = make_mesh_factory_obstacle(reg);
        set_required_shape_tag(reg, parent, Shape::Square);
        reg.emplace<int8_t>(parent, int8_t{-1});
        reg.emplace<SplitPathTag>(parent);

        CHECK_THROWS_AS(spawn_obstacle_meshes(reg, parent), std::logic_error);
        check_no_mesh_children(reg, parent);
    }

    SECTION("lane beyond lane table") {
        entt::registry reg;
        auto parent = make_mesh_factory_obstacle(reg);
        set_required_shape_tag(reg, parent, Shape::Square);
        reg.emplace<int8_t>(parent, int8_t{3});
        reg.emplace<SplitPathTag>(parent);

        CHECK_THROWS_AS(spawn_obstacle_meshes(reg, parent), std::logic_error);
        check_no_mesh_children(reg, parent);
    }
}

TEST_CASE("entity: spawn_obstacle rejects invalid ShapeGate shape before indexing color",
          "[archetype][validation]") {
    entt::registry reg;
    const Shape invalid_shape = static_cast<Shape>(255);

    CHECK_THROWS_AS(spawn_shape_gate_obstacle(reg, {360.0f, -120.0f, invalid_shape}),
                    std::logic_error);
    CHECK(count_mesh_children(reg) == 0);
}

TEST_CASE("entity: spawn_obstacle rejects invalid SplitPath lane before creating entities",
          "[archetype][validation][issue1046]") {
    entt::registry reg;

    CHECK_THROWS_AS(spawn_split_path_obstacle(reg, {360.0f, -120.0f,
                                                    Shape::Square, int8_t{3}}),
                    std::logic_error);
    CHECK(reg.view<ObstacleTag>().begin() == reg.view<ObstacleTag>().end());
    CHECK(reg.view<WorldPosition>().begin() == reg.view<WorldPosition>().end());
    CHECK(count_mesh_children(reg) == 0);
}

TEST_CASE("entity: ShapeGate Square - red color", "[archetype]") {
    entt::registry reg;
    auto e = spawn_shape_gate_obstacle(reg, {60.0f, -120.0f, Shape::Square});

    auto& c = reg.get<Color>(e);
    CHECK(c.r == 255); CHECK(c.g == 100); CHECK(c.b == 100);
}

TEST_CASE("entity: ShapeGate Triangle - green color", "[archetype]") {
    entt::registry reg;
    auto e = spawn_shape_gate_obstacle(reg, {60.0f, -120.0f, Shape::Triangle});

    auto& c = reg.get<Color>(e);
    CHECK(c.r == 100); CHECK(c.g == 255); CHECK(c.b == 100);
}

TEST_CASE("entity: SplitPath - required shape tag and required lane", "[archetype]") {
    entt::registry reg;
    auto e = spawn_split_path_obstacle(reg, {360.0f, -120.0f,
                                              Shape::Square, int8_t{2}});

    REQUIRE(reg.all_of<ObstacleTag, Vector2, WorldPosition, Obstacle, int8_t, DrawSize, Color>(e));
    REQUIRE(has_required_shape_tag(reg, e));
    CHECK(!reg.all_of<uint8_t>(e));

    CHECK(reg.get<Obstacle>(e).base_points == int16_t{constants::PTS_SPLIT_PATH});
    CHECK(current_required_shape(reg, e) == Shape::Square);
    CHECK(reg.get<int8_t>(e) == int8_t{2});
}


TEST_CASE("entity: ShapeGate position x/y propagated from input", "[archetype]") {
    entt::registry reg;
    auto e = spawn_shape_gate_obstacle(reg, {123.5f, 456.7f});

    auto& transform = reg.get<WorldPosition>(e);
    CHECK(transform.position.x == 123.5f);
    CHECK(transform.position.y == 456.7f);
}

TEST_CASE("entity: BeatInfo emplaced when provided", "[archetype]") {
    entt::registry reg;
    const BeatInfo bi{5, 1.5f, 1.0f};
    auto e = spawn_shape_gate_rhythm(reg, {360.0f, -120.0f}, bi);

    REQUIRE(reg.all_of<BeatInfo>(e));
    CHECK_FALSE(reg.all_of<Vector2>(e));
    CHECK(reg.get<BeatInfo>(e).beat_index == 5);
    CHECK(reg.get<BeatInfo>(e).arrival_time == 1.5f);
    CHECK(reg.get<BeatInfo>(e).spawn_time == 1.0f);
}

TEST_CASE("entity: no BeatInfo when not provided", "[archetype]") {
    entt::registry reg;
    auto e = spawn_shape_gate_obstacle(reg, {360.0f, -120.0f});

    CHECK_FALSE(reg.all_of<BeatInfo>(e));
    CHECK(reg.all_of<Vector2>(e));
}
