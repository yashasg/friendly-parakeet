#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// ── motion_system ────────────────────────────────────────────
//
// Single path after #349 migration:
//   motion_view — WorldTransform+MotionVelocity (obstacles, popups, particles, player)
//                 also bridges to Position when both components are present.

TEST_CASE("motion: WorldTransform+MotionVelocity moves by velocity * dt", "[motion]") {
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<WorldTransform>(e, WorldTransform{{100.0f, 200.0f}});
    reg.emplace<MotionVelocity>(e, MotionVelocity{{10.0f, 20.0f}});

    motion_system(reg, 1.0f);

    CHECK(reg.get<WorldTransform>(e).position.x == 110.0f);
    CHECK(reg.get<WorldTransform>(e).position.y == 220.0f);
}

TEST_CASE("motion: zero MotionVelocity means no movement", "[motion]") {
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<WorldTransform>(e, WorldTransform{{100.0f, 200.0f}});
    reg.emplace<MotionVelocity>(e, MotionVelocity{{0.0f, 0.0f}});

    motion_system(reg, 1.0f);

    CHECK(reg.get<WorldTransform>(e).position.x == 100.0f);
    CHECK(reg.get<WorldTransform>(e).position.y == 200.0f);
}

// ── motion_system: WorldTransform+MotionVelocity path (modern, issue #349 target) ──

TEST_CASE("motion: WorldTransform+MotionVelocity entity moves by velocity * dt", "[motion]") {
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<WorldTransform>(e, WorldTransform{{100.0f, 200.0f}});
    reg.emplace<MotionVelocity>(e, MotionVelocity{{10.0f, 20.0f}});

    motion_system(reg, 1.0f);

    const auto& wt = reg.get<WorldTransform>(e);
    CHECK(wt.position.x == 110.0f);
    CHECK(wt.position.y == 220.0f);
}

TEST_CASE("motion: WorldTransform+MotionVelocity entity with BeatInfo is excluded", "[motion]") {
    // BeatInfo entities have their position derived from song_time in scroll_system;
    // motion_system must not double-integrate their position.
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<WorldTransform>(e, WorldTransform{{100.0f, 200.0f}});
    reg.emplace<MotionVelocity>(e, MotionVelocity{{10.0f, 20.0f}});
    reg.emplace<BeatInfo>(e, BeatInfo{0, 0.0f, 0.0f});  // marks entity as beat-authoritative

    motion_system(reg, 1.0f);

    const auto& wt = reg.get<WorldTransform>(e);
    CHECK(wt.position.x == 100.0f);  // unchanged — motion_system skips BeatInfo entities
    CHECK(wt.position.y == 200.0f);
}

TEST_CASE("motion: ObstacleScrollZ entity is excluded from motion_system", "[motion]") {
    // Defense-in-depth for freeplay LowBar/HighBar: these entities carry BOTH
    // MotionVelocity (always emplaced in spawn_obstacle) AND ObstacleScrollZ
    // (emplaced for LowBar/HighBar kinds).  scroll_system's model_view owns
    // their dt-based integration via oz.z.  motion_system must NOT also
    // advance position.y, or the entity moves 2× per frame.
    //
    // Production order (playing_systems_runner.cpp):
    //   scroll_system → motion_system
    //
    // This test fails if motion_system lacks entt::exclude<ObstacleScrollZ>.
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);
    reg.emplace<WorldTransform>(e, WorldTransform{{0.0f, 100.0f}});
    reg.emplace<MotionVelocity>(e, MotionVelocity{{0.0f, 50.0f}});
    reg.emplace<ObstacleScrollZ>(e, ObstacleScrollZ{100.0f});
    // No BeatInfo — freeplay archetype.

    // Step 1: scroll_system advances oz.z and writes position.y (dt=1.0s)
    scroll_system(reg, 1.0f);
    // Step 2: motion_system must NOT advance position.y again
    motion_system(reg, 1.0f);

    const auto& wt = reg.get<WorldTransform>(e);
    // Expected: position.y advanced by exactly 50.0f (one integration via scroll_system).
    // If motion_system also runs, position.y would be 200.0f (two integrations).
    CHECK(wt.position.y == 150.0f);
}

// ── obstacle_despawn_system ───────────────────────────────────────────

TEST_CASE("cleanup: destroys obstacles past DESTROY_Y", "[cleanup]") {
    auto reg = make_registry();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{0.0f, constants::DESTROY_Y + 10.0f}});

    obstacle_despawn_system(reg, 0.016f);

    CHECK_FALSE(reg.valid(obs));
}

TEST_CASE("cleanup: keeps obstacles above DESTROY_Y", "[cleanup]") {
    auto reg = make_registry();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{0.0f, constants::DESTROY_Y - 10.0f}});

    obstacle_despawn_system(reg, 0.016f);

    CHECK(reg.valid(obs));
}

TEST_CASE("cleanup: non-obstacle entities are untouched", "[cleanup]") {
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<WorldTransform>(e, WorldTransform{{0.0f, constants::DESTROY_Y + 100.0f}});
    // No ObstacleTag

    obstacle_despawn_system(reg, 0.016f);

    CHECK(reg.valid(e));
}

// ── game_state_system ────────────────────────────────────────

TEST_CASE("game_state: title to level select on touch", "[gamestate]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    auto btn = make_menu_button(reg, MenuActionKind::Confirm, GamePhase::Title);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK_FALSE(gs.transition_pending);
    CHECK(gs.phase == GamePhase::LevelSelect);
}

TEST_CASE("game_state: game over button choice after delay", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.5f;
    auto btn = make_menu_button(reg, MenuActionKind::GoLevelSelect, GamePhase::GameOver);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    // Button tap sets choice AND transitions in same frame
    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::LevelSelect);
}

TEST_CASE("game_state: game over ignores touch during delay", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.2f;  // within 0.4s delay
    auto btn = make_menu_button(reg, MenuActionKind::Confirm, GamePhase::GameOver);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("game_state: phase timer increments", "[gamestate]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase_timer = 0.0f;

    game_state_system(reg, 0.5f);

    CHECK(reg.ctx().get<GameState>().phase_timer == 0.5f);
}

// ── game_state: transition execution ─────────────────────────

TEST_CASE("game_state: enter_playing clears entities and creates player", "[gamestate]") {
    auto reg = make_registry();
    // Create some entities that should be cleared
    auto junk = reg.create();
    reg.emplace<ObstacleTag>(junk);
    reg.emplace<WorldTransform>(junk, WorldTransform{{0.0f, 0.0f}});

    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::Playing;

    game_state_system(reg, 0.016f);

    // Original entity should be gone (registry cleared)
    CHECK_FALSE(reg.valid(junk));

    // A new player should exist
    auto player_view = reg.view<PlayerTag>();
    int player_count = 0;
    for (auto e : player_view) { ++player_count; (void)e; }
    CHECK(player_count == 1);

    // Phase should be Playing
    CHECK(gs.phase == GamePhase::Playing);
}

TEST_CASE("game_state: enter_game_over updates high score", "[gamestate]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    score.score = 5000;
    score.high_score = 3000;

    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::GameOver;

    game_state_system(reg, 0.016f);

    CHECK(score.high_score == 5000);
    CHECK(gs.phase == GamePhase::GameOver);
}

TEST_CASE("game_state: enter_game_over pushes Crash SFX", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::GameOver;

    game_state_system(reg, 0.016f);

    auto& audio = reg.ctx().get<AudioQueue>();
    CHECK(audio.count > 0);
    CHECK(audio.queue[0] == SFX::Crash);
}

TEST_CASE("game_state: enter_game_over preserves high score if lower", "[gamestate]") {
    auto reg = make_registry();
    auto& score = reg.ctx().get<ScoreState>();
    score.score = 1000;
    score.high_score = 5000;

    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::GameOver;

    game_state_system(reg, 0.016f);

    CHECK(score.high_score == 5000);
}

TEST_CASE("game_state: paused to playing on touch", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Paused;
    auto btn = make_menu_button(reg, MenuActionKind::Confirm, GamePhase::Paused);
    press_button(reg, btn);

    game_state_system(reg, 0.016f);

    CHECK(gs.phase == GamePhase::Playing);
    CHECK(gs.phase_timer == 0.0f);
}

TEST_CASE("game_state: title stays title without touch", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Title;
    // No ButtonPressEvent in queue — no actions

    game_state_system(reg, 0.5f);

    CHECK(gs.phase == GamePhase::Title);
    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("game_state: transition to Paused sets phase", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Playing;
    gs.transition_pending = true;
    gs.next_phase = GamePhase::Paused;

    game_state_system(reg, 0.016f);

    CHECK(gs.phase == GamePhase::Paused);
    CHECK(gs.phase_timer == 0.0f);
    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("game_state: transition to Settings sets phase", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Title;
    gs.transition_pending = true;
    gs.next_phase = GamePhase::Settings;

    game_state_system(reg, 0.016f);

    CHECK(gs.phase == GamePhase::Settings);
    CHECK(gs.phase_timer == 0.0f);
    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("game_state: enter_playing resets score", "[gamestate]") {
    auto reg = make_registry();
    reg.ctx().get<ScoreState>().score = 9999;

    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::Playing;

    game_state_system(reg, 0.016f);

    CHECK(reg.ctx().get<ScoreState>().score == 0);
}

// ── scroll_system: phase guard ──────────────────────────────

TEST_CASE("scroll: no movement when not in Playing phase", "[scroll]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    auto e = reg.create();
    reg.emplace<WorldTransform>(e, WorldTransform{{100.0f, 200.0f}});
    reg.emplace<MotionVelocity>(e, MotionVelocity{{10.0f, 20.0f}});

    scroll_system(reg, 1.0f);

    CHECK(reg.get<WorldTransform>(e).position.x == 100.0f);
    CHECK(reg.get<WorldTransform>(e).position.y == 200.0f);
}

TEST_CASE("motion: no movement when not in Playing phase", "[motion]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    auto e = reg.create();
    reg.emplace<WorldTransform>(e, WorldTransform{{100.0f, 200.0f}});
    reg.emplace<MotionVelocity>(e, MotionVelocity{{10.0f, 20.0f}});

    tick_playing_systems(reg, 1.0f);

    CHECK(reg.get<WorldTransform>(e).position.x == 100.0f);
    CHECK(reg.get<WorldTransform>(e).position.y == 200.0f);
}

// ── cleanup: edge cases ─────────────────────────────────────

TEST_CASE("cleanup: obstacle at exactly DESTROY_Y is kept", "[cleanup]") {
    auto reg = make_registry();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<WorldTransform>(obs, WorldTransform{{0.0f, constants::DESTROY_Y}});

    obstacle_despawn_system(reg, 0.016f);

    CHECK(reg.valid(obs));
}

// #242 — static buffer must handle multiple entities in one pass.
TEST_CASE("cleanup: destroys multiple obstacles past DESTROY_Y in one pass", "[cleanup]") {
    auto reg = make_registry();

    constexpr int N = 5;
    entt::entity obs[N];
    for (int i = 0; i < N; ++i) {
        obs[i] = reg.create();
        reg.emplace<ObstacleTag>(obs[i]);
        reg.emplace<WorldTransform>(obs[i], WorldTransform{{0.0f, constants::DESTROY_Y + static_cast<float>(i + 1) * 10.0f}});
    }

    obstacle_despawn_system(reg, 0.016f);

    for (int i = 0; i < N; ++i)
        CHECK_FALSE(reg.valid(obs[i]));
}

// #242 / #280 — cleanup must not emplace MissTag or ScoredTag; that is miss_detection_system's job.
TEST_CASE("cleanup: does not emplace MissTag or ScoredTag on surviving obstacles", "[cleanup]") {
    auto reg = make_registry();

    auto survivor = reg.create();
    reg.emplace<ObstacleTag>(survivor);
    reg.emplace<WorldTransform>(survivor, WorldTransform{{0.0f, constants::DESTROY_Y - 1.0f}});

    obstacle_despawn_system(reg, 0.016f);

    CHECK(reg.valid(survivor));
    CHECK_FALSE(reg.all_of<MissTag>(survivor));
    CHECK_FALSE(reg.all_of<ScoredTag>(survivor));
}
