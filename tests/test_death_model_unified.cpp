#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

TEST_CASE("death_model: one miss drains energy without immediate GameOver", "[death_model]") {
    auto reg = make_rhythm_registry();
    make_player(reg);

    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = constants::ENERGY_MAX;

    const auto obstacle = make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);

    CHECK(reg.all_of<MissTag>(obstacle));
    CHECK(reg.all_of<ScoredTag>(obstacle));
    CHECK(reg.ctx().get<SongResults>().miss_count == 1);
    CHECK_THAT(energy.energy,
               Catch::Matchers::WithinAbs(constants::ENERGY_MAX - constants::ENERGY_DRAIN_MISS,
                                            0.0001f));
    CHECK(energy.flash_timer == constants::ENERGY_FLASH_DURATION);
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);

    energy_system(reg, 0.016f);

    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("death_model: GameOver is deferred to energy depletion", "[death_model]") {
    auto reg = make_rhythm_registry();
    make_player(reg);

    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = constants::ENERGY_MAX;

    // ENERGY_DRAIN_MISS = 0.20, ENERGY_MAX = 1.0: 4 misses leaves 0.2 remaining,
    // which does not trigger GameOver. A 5th miss depletes to exactly 0.0.
    static constexpr int SAFE_MISS_COUNT = static_cast<int>(
        constants::ENERGY_MAX / constants::ENERGY_DRAIN_MISS) - 1;  // 4

    for (int i = 0; i < SAFE_MISS_COUNT; ++i) {
        make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);
        collision_system(reg, 0.016f);
        energy_system(reg, 0.016f);
    }

    CHECK_THAT(energy.energy,
               Catch::Matchers::WithinAbs(
                   constants::ENERGY_MAX - static_cast<float>(SAFE_MISS_COUNT) * constants::ENERGY_DRAIN_MISS,
                   0.0001f));
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);

    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);

    CHECK(energy.energy == 0.0f);
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);

    energy_system(reg, 0.016f);

    const auto& state = reg.ctx().get<GameState>();
    CHECK(state.transition_pending);
    CHECK(state.next_phase == GamePhase::GameOver);
    CHECK_FALSE(reg.ctx().get<SongState>().playing);
}

TEST_CASE("death_model: timing recovery can preserve survival margin", "[death_model]") {
    auto reg = make_rhythm_registry();
    make_player(reg);

    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.15f;

    auto scored = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<ScoredTag>(scored);
    reg.emplace<TimingGrade>(scored, TimingTier::Perfect, 1.0f);
    scoring_system(reg, 0.016f);

    CHECK_THAT(energy.energy,
               Catch::Matchers::WithinAbs(0.15f + constants::ENERGY_RECOVER_PERFECT, 0.0001f));

    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK_THAT(energy.energy,
               Catch::Matchers::WithinAbs(0.15f + constants::ENERGY_RECOVER_PERFECT -
                                               constants::ENERGY_DRAIN_MISS,
                                           0.0001f));
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("death_model: Burnout Dead-zone hit banks points without instant GameOver", "[death_model]") {
    auto reg = make_rhythm_registry();
    make_player(reg);

    auto obstacle = reg.create();
    reg.emplace<ObstacleTag>(obstacle);
    reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::PLAYER_Y - 10.0f);
    reg.emplace<Velocity>(obstacle, 0.0f, 0.0f);
    reg.emplace<Obstacle>(obstacle, ObstacleKind::ShapeGate, int16_t{constants::PTS_SHAPE_GATE});
    reg.emplace<RequiredShape>(obstacle, Shape::Circle);

    burnout_system(reg, 0.016f);
    CHECK(reg.ctx().get<BurnoutState>().zone == BurnoutZone::Dead);

    collision_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obstacle));
    CHECK(reg.ctx().get<EnergyState>().energy == constants::ENERGY_MAX);
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("death_model: Burnout Dead-zone miss drains energy instead of instant GameOver", "[death_model]") {
    auto reg = make_rhythm_registry();
    make_player(reg);

    auto obstacle = reg.create();
    reg.emplace<ObstacleTag>(obstacle);
    reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::PLAYER_Y - 10.0f);
    reg.emplace<Velocity>(obstacle, 0.0f, 0.0f);
    reg.emplace<Obstacle>(obstacle, ObstacleKind::ShapeGate, int16_t{constants::PTS_SHAPE_GATE});
    reg.emplace<RequiredShape>(obstacle, Shape::Triangle);

    burnout_system(reg, 0.016f);
    CHECK(reg.ctx().get<BurnoutState>().zone == BurnoutZone::Dead);

    collision_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK(reg.all_of<MissTag>(obstacle));
    CHECK_THAT(reg.ctx().get<EnergyState>().energy,
               Catch::Matchers::WithinAbs(constants::ENERGY_MAX - constants::ENERGY_DRAIN_MISS,
                                           0.0001f));
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("death_model: collision clamps depleted energy before GameOver transition", "[death_model]") {
    auto reg = make_rhythm_registry();
    make_player(reg);

    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.10f;

    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);

    CHECK(energy.energy == 0.0f);
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);

    energy_system(reg, 0.016f);

    CHECK(reg.ctx().get<GameState>().transition_pending);
    CHECK(reg.ctx().get<GameState>().next_phase == GamePhase::GameOver);
}

TEST_CASE("death_model: cleanup miss drains energy without directly requesting GameOver", "[death_model]") {
    auto reg = make_rhythm_registry();

    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.10f;

    auto obstacle = reg.create();
    reg.emplace<ObstacleTag>(obstacle);
    reg.emplace<Position>(obstacle, 0.0f, constants::DESTROY_Y + 10.0f);
    reg.emplace<Obstacle>(obstacle, ObstacleKind::ShapeGate, int16_t{constants::PTS_SHAPE_GATE});

    cleanup_system(reg, 0.016f);

    CHECK(reg.valid(obstacle));
    CHECK(reg.all_of<MissTag>(obstacle));
    CHECK(reg.all_of<ScoredTag>(obstacle));
    CHECK(energy.energy == 0.0f);
    CHECK(reg.ctx().get<SongResults>().miss_count == 1);
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);

    energy_system(reg, 0.016f);

    CHECK(reg.ctx().get<GameState>().transition_pending);
    CHECK(reg.ctx().get<GameState>().next_phase == GamePhase::GameOver);
}
