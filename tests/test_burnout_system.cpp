#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

TEST_CASE("burnout: no threat when no obstacles", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);

    burnout_system(reg, 0.016f);

    auto& bs = reg.ctx().get<BurnoutState>();
    CHECK((bs.nearest_threat == entt::null));
    CHECK(bs.zone == BurnoutZone::None);
    CHECK(bs.meter == 0.0f);
}

TEST_CASE("burnout: no threat when no player", "[burnout]") {
    auto reg = make_registry();
    make_shape_gate(reg, Shape::Circle, 500.0f);

    burnout_system(reg, 0.016f);

    CHECK((reg.ctx().get<BurnoutState>().nearest_threat == entt::null));
}

TEST_CASE("burnout: safe zone for distant obstacle", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    // Obstacle 600px above player → within safe zone
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 600.0f);

    burnout_system(reg, 0.016f);

    auto& bs = reg.ctx().get<BurnoutState>();
    CHECK((bs.nearest_threat != entt::null));
    CHECK(bs.zone == BurnoutZone::Safe);
    CHECK(bs.meter > 0.0f);
    CHECK(bs.meter <= 0.4f);
}

TEST_CASE("burnout: risky zone for medium obstacle", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 400.0f);

    burnout_system(reg, 0.016f);

    auto& bs = reg.ctx().get<BurnoutState>();
    CHECK(bs.zone == BurnoutZone::Risky);
    CHECK(bs.meter > 0.4f);
    CHECK(bs.meter <= 0.7f);
}

TEST_CASE("burnout: danger zone for close obstacle", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 200.0f);

    burnout_system(reg, 0.016f);

    auto& bs = reg.ctx().get<BurnoutState>();
    CHECK(bs.zone == BurnoutZone::Danger);
    CHECK(bs.meter > 0.7f);
}

TEST_CASE("burnout: none zone for very distant obstacle", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 800.0f);

    burnout_system(reg, 0.016f);

    auto& bs = reg.ctx().get<BurnoutState>();
    CHECK(bs.zone == BurnoutZone::None);
    CHECK(bs.meter == 0.0f);
}

TEST_CASE("burnout: scored obstacles are ignored", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    auto obs = make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 300.0f);
    reg.emplace<ScoredTag>(obs);

    burnout_system(reg, 0.016f);

    CHECK((reg.ctx().get<BurnoutState>().nearest_threat == entt::null));
}

TEST_CASE("burnout: nearest obstacle is tracked", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    auto far  = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y - 600.0f);
    auto near = make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 300.0f);

    burnout_system(reg, 0.016f);

    CHECK(reg.ctx().get<BurnoutState>().nearest_threat == near);
}

TEST_CASE("burnout: zone change pushes SFX", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 400.0f);

    burnout_system(reg, 0.016f);

    // Entering Risky from None should push ZoneRisky SFX
    CHECK(reg.ctx().get<AudioQueue>().count > 0);
}

TEST_CASE("burnout: obstacle below player is ignored", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    // Obstacle BELOW player (already passed)
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y + 100.0f);

    burnout_system(reg, 0.016f);

    CHECK((reg.ctx().get<BurnoutState>().nearest_threat == entt::null));
}
