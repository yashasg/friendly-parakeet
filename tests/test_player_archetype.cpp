#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include "test_helpers.h"
#include "entities/player_entity.h"

// ── create_player_entity: canonical component set and initial values ──────────
//
// These tests lock in the player entity-factory contract so divergence between
// play_session.cpp and test helpers is caught at compile/test time.

TEST_CASE("player_entity: canonical component set present", "[archetype][player]") {
    auto reg = make_registry();
    auto p = create_player_entity(reg);

    CHECK(reg.all_of<PlayerTag>(p));
    CHECK(reg.all_of<WorldTransform>(p));
    CHECK_FALSE(reg.all_of<Position>(p));
    CHECK(reg.all_of<PlayerShape>(p));
    CHECK(reg.all_of<ShapeWindow>(p));
    CHECK(reg.all_of<Lane>(p));
    CHECK(reg.all_of<VerticalState>(p));
    CHECK(reg.all_of<Color>(p));
    CHECK(reg.all_of<DrawSize>(p));
    CHECK(reg.all_of<DrawLayer>(p));
    CHECK(reg.all_of<TagWorldPass>(p));
}

TEST_CASE("player_entity: starts at center lane position", "[archetype][player]") {
    auto reg = make_registry();
    auto p = create_player_entity(reg);

    auto& transform = reg.get<WorldTransform>(p);
    CHECK(transform.position.x == constants::LANE_X[1]);
    CHECK(transform.position.y == constants::PLAYER_Y);
}

TEST_CASE("player_entity: initial shape is Hexagon", "[archetype][player]") {
    auto reg = make_registry();
    auto p = create_player_entity(reg);

    auto& ps = reg.get<PlayerShape>(p);
    CHECK(ps.current  == Shape::Hexagon);
    CHECK(ps.previous == Shape::Hexagon);
}

TEST_CASE("player_entity: ShapeWindow starts Idle targeting Hexagon", "[archetype][player]") {
    auto reg = make_registry();
    auto p = create_player_entity(reg);

    auto& sw = reg.get<ShapeWindow>(p);
    CHECK(sw.target_shape == Shape::Hexagon);
    CHECK(sw.phase        == WindowPhase::Idle);
}

TEST_CASE("player_entity: DrawLayer is Game", "[archetype][player]") {
    auto reg = make_registry();
    auto p = create_player_entity(reg);
    CHECK(reg.get<DrawLayer>(p).layer == Layer::Game);
}

TEST_CASE("player_entity: DrawSize matches PLAYER_SIZE", "[archetype][player]") {
    auto reg = make_registry();
    auto p = create_player_entity(reg);

    auto& ds = reg.get<DrawSize>(p);
    CHECK(ds.w == constants::PLAYER_SIZE);
    CHECK(ds.h == constants::PLAYER_SIZE);
}

TEST_CASE("player_entity: Lane defaults to center (1), Grounded vertical", "[archetype][player]") {
    auto reg = make_registry();
    auto p = create_player_entity(reg);

    CHECK(reg.get<Lane>(p).current        == int8_t{1});
    CHECK(reg.get<VerticalState>(p).mode  == VMode::Grounded);
}

TEST_CASE("player_entity: rejects duplicate canonical players", "[archetype][player]") {
    auto reg = make_registry();
    create_player_entity(reg);

    CHECK_THROWS_AS(create_player_entity(reg), std::logic_error);
}
