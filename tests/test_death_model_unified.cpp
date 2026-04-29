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

    // collision_system stamps MissTag+ScoredTag; energy effects are deferred to scoring_system.
    CHECK(reg.all_of<MissTag>(obstacle));
    CHECK(reg.all_of<ScoredTag>(obstacle));

    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK(reg.ctx().get<SongResults>().miss_count == 1);
    CHECK_THAT(energy.energy,
               Catch::Matchers::WithinAbs(constants::ENERGY_MAX - constants::ENERGY_DRAIN_MISS,
                                            0.0001f));
    CHECK(energy.flash_timer > 0.0f);
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);

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
        scoring_system(reg, 0.016f);
        energy_system(reg, 0.016f);
    }

    CHECK_THAT(energy.energy,
               Catch::Matchers::WithinAbs(
                   constants::ENERGY_MAX - static_cast<float>(SAFE_MISS_COUNT) * constants::ENERGY_DRAIN_MISS,
                   0.0001f));
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);

    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK(energy.energy == 0.0f);
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);

    game_state_system(reg, 0.016f);
    game_state_system(reg, 0.016f);

    const auto& state = reg.ctx().get<GameState>();
    CHECK(state.phase == GamePhase::GameOver);
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
    energy_system(reg, 0.016f);

    CHECK_THAT(energy.energy,
               Catch::Matchers::WithinAbs(0.15f + constants::ENERGY_RECOVER_PERFECT, 0.0001f));

    make_shape_gate(reg, Shape::Triangle, constants::PLAYER_Y);
    collision_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK_THAT(energy.energy,
               Catch::Matchers::WithinAbs(0.15f + constants::ENERGY_RECOVER_PERFECT -
                                               constants::ENERGY_DRAIN_MISS,
                                           0.0001f));
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("death_model: close hit banks points without instant GameOver", "[death_model]") {
    auto reg = make_rhythm_registry();
    make_player(reg);

    auto obstacle = reg.create();
    reg.emplace<ObstacleTag>(obstacle);
    reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::PLAYER_Y - 10.0f);
    reg.emplace<Velocity>(obstacle, 0.0f, 0.0f);
    reg.emplace<Obstacle>(obstacle, ObstacleKind::ShapeGate, int16_t{constants::PTS_SHAPE_GATE});
    reg.emplace<RequiredShape>(obstacle, Shape::Circle);

    collision_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK(reg.all_of<ScoredTag>(obstacle));
    CHECK(reg.ctx().get<EnergyState>().energy == constants::ENERGY_MAX);
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("death_model: close miss drains energy instead of instant GameOver", "[death_model]") {
    auto reg = make_rhythm_registry();
    make_player(reg);

    auto obstacle = reg.create();
    reg.emplace<ObstacleTag>(obstacle);
    reg.emplace<Position>(obstacle, constants::LANE_X[1], constants::PLAYER_Y - 10.0f);
    reg.emplace<Velocity>(obstacle, 0.0f, 0.0f);
    reg.emplace<Obstacle>(obstacle, ObstacleKind::ShapeGate, int16_t{constants::PTS_SHAPE_GATE});
    reg.emplace<RequiredShape>(obstacle, Shape::Triangle);

    collision_system(reg, 0.016f);
    // collision_system stamps MissTag; verify before scoring_system removes it.
    CHECK(reg.all_of<MissTag>(obstacle));
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

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
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK(energy.energy == 0.0f);
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);

    game_state_system(reg, 0.016f);
    CHECK(reg.ctx().get<GameState>().transition_pending);
}

TEST_CASE("death_model: scroll-past miss drains energy without directly requesting GameOver", "[death_model]") {
    auto reg = make_rhythm_registry();

    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 0.10f;

    auto obstacle = reg.create();
    reg.emplace<ObstacleTag>(obstacle);
    reg.emplace<Position>(obstacle, 0.0f, constants::DESTROY_Y + 10.0f);
    reg.emplace<Obstacle>(obstacle, ObstacleKind::ShapeGate, int16_t{constants::PTS_SHAPE_GATE});

    // miss_detection_system tags; scoring_system drains.
    miss_detection_system(reg, 0.016f);

    CHECK(reg.valid(obstacle));
    CHECK(reg.all_of<MissTag>(obstacle));
    CHECK(reg.all_of<ScoredTag>(obstacle));
    // Energy not yet drained — scoring_system hasn't run.
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);

    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK(energy.energy == 0.0f);
    CHECK(reg.ctx().get<SongResults>().miss_count == 1);
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);

    game_state_system(reg, 0.016f);
    CHECK(reg.ctx().get<GameState>().transition_pending);
}

// Regression: a scroll-past miss that depletes energy to 0 triggers GameOver
// in the same frame (miss_detection → scoring → energy run in order).
TEST_CASE("death_model: scroll-past fatal miss triggers GameOver same frame", "[death_model]") {
    auto reg = make_rhythm_registry();

    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = constants::ENERGY_DRAIN_MISS;  // exactly one miss away from death

    auto obstacle = reg.create();
    reg.emplace<ObstacleTag>(obstacle);
    reg.emplace<Position>(obstacle, 0.0f, constants::DESTROY_Y + 10.0f);
    reg.emplace<Obstacle>(obstacle, ObstacleKind::ShapeGate, int16_t{constants::PTS_SHAPE_GATE});

    miss_detection_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK(energy.energy == 0.0f);
    CHECK_FALSE(reg.ctx().get<GameState>().transition_pending);

    game_state_system(reg, 0.016f);
    CHECK(reg.ctx().get<GameState>().transition_pending);
}

// Regression: a scroll-past miss must not double-drain (energy delta = exactly
// -ENERGY_DRAIN_MISS, miss_count delta = 1).
TEST_CASE("death_model: scroll-past miss does not double-drain", "[death_model]") {
    auto reg = make_rhythm_registry();

    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = constants::ENERGY_MAX;

    auto obstacle = reg.create();
    reg.emplace<ObstacleTag>(obstacle);
    reg.emplace<Position>(obstacle, 0.0f, constants::DESTROY_Y + 10.0f);
    reg.emplace<Obstacle>(obstacle, ObstacleKind::ShapeGate, int16_t{constants::PTS_SHAPE_GATE});

    miss_detection_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK(reg.ctx().get<SongResults>().miss_count == 1);
    CHECK_THAT(energy.energy,
               Catch::Matchers::WithinAbs(constants::ENERGY_MAX - constants::ENERGY_DRAIN_MISS,
                                          0.0001f));
}

// Regression: a LanePush that scrolls past DESTROY_Y must not produce a MissTag
// or any energy delta (ScoredTag from collision is the guard).
TEST_CASE("death_model: LanePush scroll-past does not produce MissTag", "[death_model]") {
    auto reg = make_rhythm_registry();
    make_player(reg);

    auto& energy = reg.ctx().get<EnergyState>();
    const float initial_energy = energy.energy;

    auto obstacle = reg.create();
    reg.emplace<ObstacleTag>(obstacle);
    reg.emplace<Position>(obstacle, 0.0f, constants::DESTROY_Y + 10.0f);
    reg.emplace<Obstacle>(obstacle, ObstacleKind::LanePushLeft, int16_t{0});
    reg.emplace<ScoredTag>(obstacle);  // collision_system stamps ScoredTag for LanePush at pass-through

    miss_detection_system(reg, 0.016f);
    scoring_system(reg, 0.016f);
    energy_system(reg, 0.016f);

    CHECK_FALSE(reg.all_of<MissTag>(obstacle));
    CHECK_THAT(energy.energy, Catch::Matchers::WithinAbs(initial_energy, 0.0001f));
    CHECK(reg.ctx().get<SongResults>().miss_count == 0);
}
