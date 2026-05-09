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
    reg.emplace<WorldTransform>(obs, WorldTransform{{0.0f, constants::DESTROY_Y + 1.0f}});

    tick_playing_systems(reg, 0.016f);

    CHECK(score.score  == score_before);
    CHECK(energy.energy == energy_before);
    CHECK_FALSE(reg.all_of<MissTag>(obs));
    CHECK_FALSE(reg.all_of<ScoredTag>(obs));
}

TEST_CASE("tick_playing_systems: no-op when phase is GameOver", "[phase_guard]") {
    auto reg = make_rhythm_registry();
    reg.ctx().get<GameState>().phase = GamePhase::GameOver;

    // Expired obstacle to match Paused coverage (MissTag / ScoredTag guard).
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{0.0f, constants::DESTROY_Y + 1.0f}});

    auto& score = reg.ctx().get<ScoreState>();
    auto& energy = reg.ctx().get<EnergyState>();
    const int   score_before  = score.score;
    const float energy_before = energy.energy;

    tick_playing_systems(reg, 0.016f);

    CHECK(score.score   == score_before);
    CHECK(energy.energy == energy_before);
    CHECK_FALSE(reg.all_of<MissTag>(obs));
    CHECK_FALSE(reg.all_of<ScoredTag>(obs));
}

// ── tick_fixed_systems: score-feedback chain wiring guard ────────────────────
//
// Verifies that popup_feedback_system and energy_system are wired in
// tick_fixed_systems (NOT inside tick_playing_systems / the runner).
//
// Why placement matters: scoring_system (inside tick_playing_systems) populates
// ScorePopupRequestQueue; popup_feedback_system must run AFTER tick_playing_systems
// so the queue is already filled when popup_feedback consumes it.  Moving them
// INTO tick_playing_systems would run them before scoring_system, silently
// dropping all popups.
//
// Why relative ordering between obstacle_despawn and popup_feedback does NOT
// matter (Keaton-r14 finding): these two systems are commutative.
//   obstacle_despawn reads ObstacleTag+WorldTransform → destroys entities
//   popup_feedback  reads ScorePopupRequestQueue (ctx) → creates popup entities
// Their data surfaces are disjoint; no observable state diff results from
// swapping them.  The relative call order in fixed_tick_runner.cpp is a
// cache-locality preference, not a semantic invariant.
//
// This test guards against systems being silently DROPPED from tick_fixed_systems
// (the primary wiring regression risk).  The call-order comment in
// fixed_tick_runner.cpp is the documentation anchor for cache-locality rationale.
TEST_CASE("tick_fixed_systems: popup_feedback and energy run in score-feedback chain", "[phase_guard][integration][order_regression]") {
    auto reg = make_rhythm_registry();
    // Phase must be Playing for popup_feedback and energy guards to pass.
    reg.ctx().get<GameState>().phase = GamePhase::Playing;

    // Pre-seed a popup request directly (bypasses scoring_system path; tests
    // that popup_feedback_system is wired and consumes the queue).
    auto& queue = reg.ctx().emplace<ScorePopupRequestQueue>();
    queue.requests.push_back({100.0f, 200.0f, 10, std::nullopt});

    // Pre-seed a pending energy effect to verify energy_system is wired.
    auto& pending = reg.ctx().emplace<PendingEnergyEffects>();
    pending.events.push_back({20.0f, false});

    // Set energy to 0 so the +20 effect is visible after clamping.
    reg.ctx().get<EnergyState>().energy = 0.0f;

    tick_fixed_systems(reg, 0.016f);

    // popup_feedback_system must have consumed the queue and spawned an entity.
    CHECK(queue.requests.empty());
    const auto popup_view = reg.view<ScorePopup>();
    CHECK_FALSE(popup_view.empty());

    // energy_system must have applied the pending effect.
    CHECK(reg.ctx().get<EnergyState>().energy > 0.0f);

    // Pending events must be cleared.
    CHECK(pending.events.empty());
}
