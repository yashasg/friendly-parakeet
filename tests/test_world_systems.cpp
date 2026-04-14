#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// ── scroll_system ────────────────────────────────────────────

TEST_CASE("scroll: entities move by velocity * dt", "[scroll]") {
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<Position>(e, 100.0f, 200.0f);
    reg.emplace<Velocity>(e, 10.0f, 20.0f);

    scroll_system(reg, 1.0f);

    CHECK(reg.get<Position>(e).x == 110.0f);
    CHECK(reg.get<Position>(e).y == 220.0f);
}

TEST_CASE("scroll: zero velocity means no movement", "[scroll]") {
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<Position>(e, 100.0f, 200.0f);
    reg.emplace<Velocity>(e, 0.0f, 0.0f);

    scroll_system(reg, 1.0f);

    CHECK(reg.get<Position>(e).x == 100.0f);
    CHECK(reg.get<Position>(e).y == 200.0f);
}

TEST_CASE("scroll: multiple entities updated", "[scroll]") {
    auto reg = make_registry();
    auto e1 = reg.create();
    reg.emplace<Position>(e1, 0.0f, 0.0f);
    reg.emplace<Velocity>(e1, 1.0f, 0.0f);
    auto e2 = reg.create();
    reg.emplace<Position>(e2, 0.0f, 0.0f);
    reg.emplace<Velocity>(e2, 0.0f, 1.0f);

    scroll_system(reg, 10.0f);

    CHECK(reg.get<Position>(e1).x == 10.0f);
    CHECK(reg.get<Position>(e2).y == 10.0f);
}

// ── cleanup_system ───────────────────────────────────────────

TEST_CASE("cleanup: destroys obstacles past DESTROY_Y", "[cleanup]") {
    auto reg = make_registry();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, 0.0f, constants::DESTROY_Y + 10.0f);

    cleanup_system(reg, 0.016f);

    CHECK_FALSE(reg.valid(obs));
}

TEST_CASE("cleanup: keeps obstacles above DESTROY_Y", "[cleanup]") {
    auto reg = make_registry();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, 0.0f, constants::DESTROY_Y - 10.0f);

    cleanup_system(reg, 0.016f);

    CHECK(reg.valid(obs));
}

TEST_CASE("cleanup: non-obstacle entities are untouched", "[cleanup]") {
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<Position>(e, 0.0f, constants::DESTROY_Y + 100.0f);
    // No ObstacleTag

    cleanup_system(reg, 0.016f);

    CHECK(reg.valid(e));
}

// ── lifetime_system ──────────────────────────────────────────

TEST_CASE("lifetime: entity destroyed when timer expires", "[lifetime]") {
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<Lifetime>(e, 0.5f, 1.0f);

    lifetime_system(reg, 0.6f);

    CHECK_FALSE(reg.valid(e));
}

TEST_CASE("lifetime: entity survives when timer still positive", "[lifetime]") {
    auto reg = make_registry();
    auto e = reg.create();
    reg.emplace<Lifetime>(e, 1.0f, 1.0f);

    lifetime_system(reg, 0.3f);

    CHECK(reg.valid(e));
    CHECK(reg.get<Lifetime>(e).remaining < 1.0f);
}

TEST_CASE("lifetime: multiple entities processed", "[lifetime]") {
    auto reg = make_registry();
    auto e1 = reg.create();
    reg.emplace<Lifetime>(e1, 0.1f, 1.0f);
    auto e2 = reg.create();
    reg.emplace<Lifetime>(e2, 2.0f, 2.0f);

    lifetime_system(reg, 0.5f);

    CHECK_FALSE(reg.valid(e1));
    CHECK(reg.valid(e2));
}

// ── obstacle_spawn_system ────────────────────────────────────

TEST_CASE("spawn: spawns obstacle when timer expires", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.spawn_timer = 0.0f;  // ready to spawn

    int before = 0;
    for (auto e : reg.view<ObstacleTag>()) { ++before; (void)e; }

    obstacle_spawn_system(reg, 0.016f);

    int after = 0;
    for (auto e : reg.view<ObstacleTag>()) { ++after; (void)e; }
    CHECK(after == before + 1);
}

TEST_CASE("spawn: no spawn when timer still positive", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.spawn_timer = 5.0f;

    obstacle_spawn_system(reg, 0.016f);

    int count = 0;
    for (auto e : reg.view<ObstacleTag>()) { ++count; (void)e; }
    CHECK(count == 0);
}

TEST_CASE("spawn: only ShapeGate at t=0", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.elapsed = 0.0f;
    config.spawn_timer = 0.0f;

    // Spawn many and check all are ShapeGate
    for (int i = 0; i < 20; ++i) {
        config.spawn_timer = 0.0f;
        obstacle_spawn_system(reg, 0.016f);
    }

    auto view = reg.view<ObstacleTag, Obstacle>();
    for (auto [e, obs] : view.each()) {
        CHECK(obs.kind == ObstacleKind::ShapeGate);
    }
}

TEST_CASE("spawn: obstacles have position at SPAWN_Y", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.spawn_timer = 0.0f;

    obstacle_spawn_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Position>();
    for (auto [e, pos] : view.each()) {
        CHECK(pos.y == constants::SPAWN_Y);
    }
}

// ── game_state_system ────────────────────────────────────────

TEST_CASE("game_state: title to level select on touch", "[gamestate]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Confirm);

    game_state_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::LevelSelect);
}

TEST_CASE("game_state: game over button choice after delay", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.5f;
    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Position, constants::SCREEN_W / 2.0f, 940.0f);

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
    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Confirm);

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
    reg.emplace<Position>(junk, 0.0f, 0.0f);

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
    auto& aq = reg.ctx().get<ActionQueue>();
    aq.tap(Button::Confirm);

    game_state_system(reg, 0.016f);

    CHECK(gs.phase == GamePhase::Playing);
    CHECK(gs.phase_timer == 0.0f);
}

TEST_CASE("game_state: title stays title without touch", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Title;
    // Empty ActionQueue — no actions

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

TEST_CASE("game_state: enter_playing resets difficulty config", "[gamestate]") {
    auto reg = make_registry();
    // Modify difficulty to simulate mid-game state
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.elapsed = 100.0f;
    config.speed_multiplier = 2.5f;

    auto& gs = reg.ctx().get<GameState>();
    gs.transition_pending = true;
    gs.next_phase = GamePhase::Playing;

    game_state_system(reg, 0.016f);

    auto& new_config = reg.ctx().get<DifficultyConfig>();
    CHECK(new_config.elapsed == 0.0f);
    CHECK(new_config.speed_multiplier == 1.0f);
    CHECK(new_config.spawn_interval == constants::INITIAL_SPAWN_INT);
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
    reg.emplace<Position>(e, 100.0f, 200.0f);
    reg.emplace<Velocity>(e, 10.0f, 20.0f);

    scroll_system(reg, 1.0f);

    CHECK(reg.get<Position>(e).x == 100.0f);
    CHECK(reg.get<Position>(e).y == 200.0f);
}

// ── cleanup: edge cases ─────────────────────────────────────

TEST_CASE("cleanup: obstacle at exactly DESTROY_Y is kept", "[cleanup]") {
    auto reg = make_registry();
    auto obs = reg.create();
    reg.emplace<ObstacleTag>(obs);
    reg.emplace<Position>(obs, 0.0f, constants::DESTROY_Y);

    cleanup_system(reg, 0.016f);

    CHECK(reg.valid(obs));
}

// ── obstacle_spawn: phase guard ─────────────────────────────

TEST_CASE("spawn: no spawn when not in Playing phase", "[spawn]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.spawn_timer = 0.0f;

    obstacle_spawn_system(reg, 0.016f);

    int count = 0;
    for (auto e : reg.view<ObstacleTag>()) { ++count; (void)e; }
    CHECK(count == 0);
}

TEST_CASE("spawn: timer resets to spawn_interval after spawning", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.spawn_timer = 0.0f;
    config.spawn_interval = 1.5f;

    obstacle_spawn_system(reg, 0.016f);

    // Timer should be close to spawn_interval (minus the dt that triggered the spawn)
    CHECK(config.spawn_timer > 0.0f);
}

TEST_CASE("spawn: spawned obstacles have velocity", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.spawn_timer = 0.0f;

    obstacle_spawn_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Velocity>();
    for (auto [e, vel] : view.each()) {
        CHECK(vel.dy == config.scroll_speed);
    }
}

TEST_CASE("spawn: spawned obstacles have DrawLayer", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.spawn_timer = 0.0f;

    obstacle_spawn_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, DrawLayer>();
    int count = 0;
    for (auto e : view) { ++count; (void)e; }
    CHECK(count == 1);
}

TEST_CASE("spawn: LaneBlock available after 30s", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.elapsed = 35.0f;

    bool found_lane_block = false;
    for (int i = 0; i < 100; ++i) {
        config.spawn_timer = 0.0f;
        obstacle_spawn_system(reg, 0.016f);
    }

    auto view = reg.view<ObstacleTag, Obstacle>();
    for (auto [e, obs] : view.each()) {
        if (obs.kind == ObstacleKind::LaneBlock) found_lane_block = true;
    }
    CHECK(found_lane_block);
}

TEST_CASE("spawn: all kinds available after 120s", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.elapsed = 125.0f;

    bool found[6] = {};
    for (int i = 0; i < 500; ++i) {
        config.spawn_timer = 0.0f;
        obstacle_spawn_system(reg, 0.016f);
    }

    auto view = reg.view<ObstacleTag, Obstacle>();
    for (auto [e, obs] : view.each()) {
        found[static_cast<int>(obs.kind)] = true;
    }
    for (int i = 0; i < 6; ++i) {
        CHECK(found[i]);
    }
}
