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

TEST_CASE("game_state: title to playing on touch", "[gamestate]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::Title;
    reg.ctx().get<InputState>().touch_up = true;

    game_state_system(reg, 0.016f);

    auto& gs = reg.ctx().get<GameState>();
    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::Playing);
}

TEST_CASE("game_state: game over retry after delay", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.5f;  // past 0.4s delay
    reg.ctx().get<InputState>().touch_up = true;

    game_state_system(reg, 0.016f);

    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::Playing);
}

TEST_CASE("game_state: game over ignores touch during delay", "[gamestate]") {
    auto reg = make_registry();
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.2f;  // within 0.4s delay
    reg.ctx().get<InputState>().touch_up = true;

    game_state_system(reg, 0.016f);

    CHECK_FALSE(gs.transition_pending);
}

TEST_CASE("game_state: phase timer increments", "[gamestate]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase_timer = 0.0f;

    game_state_system(reg, 0.5f);

    CHECK(reg.ctx().get<GameState>().phase_timer == 0.5f);
}
