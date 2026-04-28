#include <catch2/catch_test_macros.hpp>
#include <stdexcept>
#include "test_helpers.h"
#include "archetypes/player_archetype.h"

// ── create_player_entity: canonical component set and initial values ──────────
//
// These tests lock in the player archetype contract so divergence between
// play_session.cpp and test helpers is caught at compile/test time.

TEST_CASE("player_archetype: canonical component set present", "[archetype][player]") {
    auto reg = make_registry();
    auto p = create_player_entity(reg);

    CHECK(reg.all_of<PlayerTag>(p));
    CHECK(reg.all_of<Position>(p));
    CHECK(reg.all_of<WorldTransform>(p));
    CHECK(reg.all_of<PlayerShape>(p));
    CHECK(reg.all_of<ShapeWindow>(p));
    CHECK(reg.all_of<Lane>(p));
    CHECK(reg.all_of<VerticalState>(p));
    CHECK(reg.all_of<Color>(p));
    CHECK(reg.all_of<DrawSize>(p));
    CHECK(reg.all_of<DrawLayer>(p));
    CHECK(reg.all_of<TagWorldPass>(p));
}

TEST_CASE("player_archetype: starts at center lane position", "[archetype][player]") {
    auto reg = make_registry();
    auto p = create_player_entity(reg);

    auto& pos = reg.get<Position>(p);
    auto& transform = reg.get<WorldTransform>(p);
    CHECK(pos.x == constants::LANE_X[1]);
    CHECK(pos.y == constants::PLAYER_Y);
    CHECK(transform.position.x == constants::LANE_X[1]);
    CHECK(transform.position.y == constants::PLAYER_Y);
}

TEST_CASE("player_archetype: initial shape is Hexagon", "[archetype][player]") {
    auto reg = make_registry();
    auto p = create_player_entity(reg);

    auto& ps = reg.get<PlayerShape>(p);
    CHECK(ps.current  == Shape::Hexagon);
    CHECK(ps.previous == Shape::Hexagon);
}

TEST_CASE("player_archetype: ShapeWindow starts Idle targeting Hexagon", "[archetype][player]") {
    auto reg = make_registry();
    auto p = create_player_entity(reg);

    auto& sw = reg.get<ShapeWindow>(p);
    CHECK(sw.target_shape == Shape::Hexagon);
    CHECK(sw.phase        == WindowPhase::Idle);
}

TEST_CASE("player_archetype: DrawLayer is Game", "[archetype][player]") {
    auto reg = make_registry();
    auto p = create_player_entity(reg);
    CHECK(reg.get<DrawLayer>(p).layer == Layer::Game);
}

TEST_CASE("player_archetype: DrawSize matches PLAYER_SIZE", "[archetype][player]") {
    auto reg = make_registry();
    auto p = create_player_entity(reg);

    auto& ds = reg.get<DrawSize>(p);
    CHECK(ds.w == constants::PLAYER_SIZE);
    CHECK(ds.h == constants::PLAYER_SIZE);
}

TEST_CASE("player_archetype: Lane defaults to center (1), Grounded vertical", "[archetype][player]") {
    auto reg = make_registry();
    auto p = create_player_entity(reg);

    CHECK(reg.get<Lane>(p).current        == int8_t{1});
    CHECK(reg.get<VerticalState>(p).mode  == VMode::Grounded);
}

TEST_CASE("player_archetype: rejects duplicate canonical players", "[archetype][player]") {
    auto reg = make_registry();
    create_player_entity(reg);

    CHECK_THROWS_AS(create_player_entity(reg), std::logic_error);
}
