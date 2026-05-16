#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "test_helpers.h"

using Catch::Matchers::WithinAbs;

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

TEST_CASE("tick_playing_systems: collision resolves before active window expiry", "[phase_guard][integration][order_regression]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    song.song_time = 10.0f + song.window_duration;
    ps.current = Shape::Circle;
    sw.target_shape = Shape::Circle;
    set_window_phase_active(reg, player);
    sw.window_start = 10.0f;
    sw.window_timer = 0.0f;
    sw.press_time = song.song_time;
    sw.graded = false;

    auto obstacle = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obstacle, 0, song.song_time, song.song_time - song.lead_time);

    tick_playing_systems(reg, 0.016f);

    CHECK(reg.ctx().get<SongResults>().perfect_count == 1);
    CHECK(window_phase_is_morph_out(reg, player));
    CHECK_THAT(sw.window_start, WithinAbs(10.0f + song.window_duration, 0.0001f));
}

TEST_CASE("tick_playing_systems: shape window activates before collision", "[phase_guard][integration][order_regression]") {
    auto reg = make_rhythm_registry();
    auto player = make_rhythm_player(reg);
    auto& ps = reg.get<PlayerShape>(player);
    auto& sw = reg.get<ShapeWindow>(player);
    auto& song = reg.ctx().get<SongState>();

    song.song_time = 10.0f;
    ps.current = Shape::Hexagon;
    sw.target_shape = Shape::Circle;
    set_window_phase_morph_in(reg, player);
    sw.window_start = song.song_time - song.morph_duration - 0.01f;
    sw.window_timer = 0.0f;
    sw.press_time = song.song_time;
    sw.graded = false;

    auto obstacle = make_shape_gate(reg, Shape::Circle, constants::PLAYER_Y);
    reg.emplace<BeatInfo>(obstacle, 0, song.song_time, song.song_time - song.lead_time);

    tick_playing_systems(reg, 0.016f);

    CHECK(window_phase_is_active(reg, player));
    CHECK(ps.current == Shape::Circle);
    CHECK(reg.ctx().get<SongResults>().perfect_count == 1);
    CHECK(reg.ctx().get<SongResults>().miss_count == 0);
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
    queue.requests.push_back({100.0f, 200.0f, 10, false, TimingTier::Ok});

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

TEST_CASE("tick_fixed_systems: energy depletion requests GameOver before next Playing pass",
          "[phase_guard][energy][regression][issue781]") {
    auto reg = make_rhythm_registry();
    auto& gs = reg.ctx().get<GameState>();
    auto& energy = reg.ctx().get<EnergyState>();
    auto& pending = reg.ctx().get<PendingEnergyEffects>();
    auto& results = reg.ctx().get<SongResults>();

    energy.energy = constants::ENERGY_DRAIN_MISS;
    pending.events.push_back({-constants::ENERGY_DRAIN_MISS, true});

    tick_fixed_systems(reg, 0.016f);

    REQUIRE(energy.energy == 0.0f);
    REQUIRE(gs.transition_pending);
    REQUIRE(gs.next_phase == GamePhase::GameOver);
    CHECK(reg.ctx().find<EnergyDepletedDeath>() != nullptr);
    CHECK(pending.events.empty());

    auto late_miss = reg.create();
    reg.emplace<ObstacleTag>(late_miss);
    reg.emplace<ScoredTag>(late_miss);
    reg.emplace<MissTag>(late_miss);
    reg.emplace<Obstacle>(late_miss, int16_t{constants::PTS_SHAPE_GATE});

    tick_fixed_systems(reg, 0.016f);

    CHECK(gs.phase == GamePhase::GameOver);
    CHECK(results.miss_count == 0);

    const auto sfx = drain_sfx_events(reg);
    CHECK(sfx.count == 1);
    CHECK(sfx.buf[0] == SFX::Crash);

    const auto haptics = drain_haptic_events(reg);
    REQUIRE(haptics.count >= 1);
    CHECK(haptics.buf[0] == HapticEvent::DeathCrash);

    tick_fixed_systems(reg, 0.016f);
    CHECK(drain_sfx_events(reg).count == 0);
    CHECK(drain_haptic_events(reg).count == 0);
}

TEST_CASE("tick_fixed_systems: fatal final drain wins after song finishes",
          "[phase_guard][energy][regression][issue961]") {
    auto reg = make_rhythm_registry();
    auto& gs = reg.ctx().get<GameState>();
    auto& song = reg.ctx().get<SongState>();
    auto& energy = reg.ctx().get<EnergyState>();
    auto& pending = reg.ctx().get<PendingEnergyEffects>();

    song.song_time = song.duration_sec;
    song.playing = true;
    song.finished = false;
    energy.energy = constants::ENERGY_DRAIN_MISS;
    pending.events.push_back({-constants::ENERGY_DRAIN_MISS, true});

    tick_fixed_systems(reg, 0.016f);

    CHECK(song.finished);
    CHECK_FALSE(song.playing);
    REQUIRE(energy.energy == 0.0f);
    REQUIRE(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::GameOver);
    CHECK(reg.ctx().find<EnergyDepletedDeath>() != nullptr);
}
