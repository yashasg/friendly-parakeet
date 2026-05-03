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

    // Expired obstacle to match Paused coverage (MissTag / ScoredTag guard).
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<ObstacleScrollZ>(obs, constants::DESTROY_Y + 1.0f);

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

// ── tick_fixed_systems: score-feedback chain order regression guard ──────────
//
// Verifies that popup_feedback_system and energy_system are wired in
// tick_fixed_systems (NOT inside tick_playing_systems / the runner) and that
// they execute AFTER obstacle_despawn_system.
//
// Why ordering matters (Keyser-r10 invariant, fixed_tick_runner.cpp:21-26):
//   scoring_system queues ScorePopupRequests → obstacle_despawn removes scored
//   obstacles → popup_feedback_system consumes the queue and spawns popup
//   entities → popup_display_system and energy_system advance feedback state.
//   Moving popup_feedback or energy INTO the runner breaks this ordering:
//   they would run before obstacle_despawn, violating the post-despawn
//   invariant documented in the comment block in tick_fixed_systems.
//
// Note on behavioral equivalence: because popup_feedback reads from a
// pre-populated queue (not live obstacle entities), wrong ordering does not
// produce a visible crash. The correct ordering is a design contract, not
// a runtime invariant detectable per-tick.  This test therefore guards
// against the systems being silently DROPPED from tick_fixed_systems (the
// primary wiring regression risk) rather than against wrong call sequence
// directly.  The comment in fixed_tick_runner.cpp is the primary ordering
// guard for code review.
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
