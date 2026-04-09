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
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y - 600.0f);
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

TEST_CASE("burnout: dead zone for very close obstacle", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    // Very close obstacle (within DANGER_MIN = 140px)
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 50.0f);

    burnout_system(reg, 0.016f);

    auto& bs = reg.ctx().get<BurnoutState>();
    CHECK(bs.zone == BurnoutZone::Dead);
    CHECK(bs.meter == 1.0f);
}

TEST_CASE("burnout: not in Playing phase skips processing", "[burnout]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    make_player(reg);
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 400.0f);

    burnout_system(reg, 0.016f);

    // State should remain default (None)
    CHECK(reg.ctx().get<BurnoutState>().zone == BurnoutZone::None);
}

TEST_CASE("burnout: window scale affects zone boundaries", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    // Place obstacle at distance that's normally in Safe zone (600px)
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 600.0f);

    // With default scale (1.0), 600px is in Safe zone
    burnout_system(reg, 0.016f);
    CHECK(reg.ctx().get<BurnoutState>().zone == BurnoutZone::Safe);

    // Shrink window scale significantly - now the same distance may be None
    reg.ctx().get<DifficultyConfig>().burnout_window_scale = 0.5f;
    // Reset to re-evaluate
    reg.ctx().get<BurnoutState>().zone = BurnoutZone::None;
    burnout_system(reg, 0.016f);

    // With scale=0.5: safe_max = 700*0.5 = 350. Distance 600 > 350 → None
    CHECK(reg.ctx().get<BurnoutState>().zone == BurnoutZone::None);
}

TEST_CASE("burnout: zone change to Danger pushes ZoneDanger SFX", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    // Set previous zone to Safe so there's a zone change
    reg.ctx().get<BurnoutState>().zone = BurnoutZone::Safe;
    // Place obstacle in Danger zone (200px above)
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 200.0f);

    burnout_system(reg, 0.016f);

    CHECK(reg.ctx().get<BurnoutState>().zone == BurnoutZone::Danger);
    auto& audio = reg.ctx().get<AudioQueue>();
    bool found_danger = false;
    for (int i = 0; i < audio.count; ++i) {
        if (audio.queue[i] == SFX::ZoneDanger) found_danger = true;
    }
    CHECK(found_danger);
}

TEST_CASE("burnout: zone change to Dead pushes ZoneDead SFX", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    // Previous zone was Danger
    reg.ctx().get<BurnoutState>().zone = BurnoutZone::Danger;
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 50.0f);

    burnout_system(reg, 0.016f);

    CHECK(reg.ctx().get<BurnoutState>().zone == BurnoutZone::Dead);
    auto& audio = reg.ctx().get<AudioQueue>();
    bool found_dead = false;
    for (int i = 0; i < audio.count; ++i) {
        if (audio.queue[i] == SFX::ZoneDead) found_dead = true;
    }
    CHECK(found_dead);
}

TEST_CASE("burnout: same zone does not push SFX", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    // Already in Risky zone
    reg.ctx().get<BurnoutState>().zone = BurnoutZone::Risky;
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 400.0f);

    burnout_system(reg, 0.016f);

    CHECK(reg.ctx().get<BurnoutState>().zone == BurnoutZone::Risky);
    CHECK(reg.ctx().get<AudioQueue>().count == 0);
}

TEST_CASE("burnout: meter range for danger zone", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 200.0f);

    burnout_system(reg, 0.016f);

    auto& bs = reg.ctx().get<BurnoutState>();
    CHECK(bs.zone == BurnoutZone::Danger);
    CHECK(bs.meter >= 0.7f);
    CHECK(bs.meter <= 0.95f);
}
