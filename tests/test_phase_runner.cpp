#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// ── tick_playing_systems: phase-gate integration ─────────────────────────────
//
// Verifies that the runner returns immediately for every non-Playing phase,
// leaving all observable state unchanged.  This is the testability hook
// enabled by Design B (phase-guard consolidation, R9).

TEST_CASE("tick_playing_systems: no-op when phase is Paused", "[phase_guard]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Paused;

    // Observable state that at least three different playing systems would mutate:
    //   scoring_system    → ScoreState::score (distance bonus each tick)
    //   energy_system     → EnergyState::display (smoothed toward energy)
    //   miss_detection_system → MissTag / ScoredTag on an expired obstacle
    auto& score        = reg.ctx().get<ScoreState>();
    auto& energy       = reg.ctx().get<EnergyState>();
    const int    score_before  = score.score;
    const float  energy_before = energy.energy;

    // Expired obstacle that miss_detection would stamp if it ran.
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<ObstacleScrollZ>(obs, constants::DESTROY_Y + 1.0f);

    tick_playing_systems(reg, 0.016f);

    CHECK(score.score  == score_before);
    CHECK(energy.energy == energy_before);
    CHECK_FALSE(reg.all_of<MissTag>(obs));
    CHECK_FALSE(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("tick_playing_systems: no-op when phase is GameOver", "[phase_guard]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<GameState>().phase = GamePhase::GameOver;

    auto& score        = reg.ctx().get<ScoreState>();
    const int score_before = score.score;

    tick_playing_systems(reg, 0.016f);

    CHECK(score.score == score_before);
}
