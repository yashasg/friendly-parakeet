// Issue #239: burnout_system removed from frame update path.
// These tests confirm the function still compiles and is truly inert —
// calling it must not change game state, push SFX, or emit haptics.

#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

TEST_CASE("burnout_system: is a no-op — zone stays None with no obstacles", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);

    burnout_system(reg, 0.016f);

    auto& bs = reg.ctx().get<BurnoutState>();
    CHECK(bs.zone == BurnoutZone::None);
    CHECK(bs.meter == 0.0f);
    CHECK((bs.nearest_threat == entt::null));
}

TEST_CASE("burnout_system: is a no-op — zone stays None even with a close obstacle", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y - 50.0f);

    burnout_system(reg, 0.016f);

    auto& bs = reg.ctx().get<BurnoutState>();
    CHECK(bs.zone == BurnoutZone::None);
    CHECK(bs.meter == 0.0f);
}

TEST_CASE("burnout_system: is a no-op — no SFX pushed regardless of obstacle distance", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y - 400.0f);

    burnout_system(reg, 0.016f);

    CHECK(reg.ctx().get<AudioQueue>().count == 0);
}

TEST_CASE("burnout_system: is a no-op — no haptics emitted", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y - 100.0f);

    burnout_system(reg, 0.016f);

    CHECK(reg.ctx().get<HapticQueue>().count == 0);
}

TEST_CASE("burnout_system: is a no-op — pre-set zone not overwritten", "[burnout]") {
    auto reg = make_registry();
    make_player(reg);
    reg.ctx().get<BurnoutState>().zone = BurnoutZone::Risky;

    burnout_system(reg, 0.016f);

    // The system does nothing, so the zone we set is unchanged.
    CHECK(reg.ctx().get<BurnoutState>().zone == BurnoutZone::Risky);
}
