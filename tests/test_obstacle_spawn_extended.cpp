#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// ── obstacle_spawn_system: rhythm mode bypass ────────────────

TEST_CASE("spawn: rhythm mode bypasses random spawning", "[spawn][rhythm]") {
    auto reg = make_rhythm_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.spawn_timer = 0.0f;
    // SongState is playing → spawning should be bypassed

    obstacle_spawn_system(reg, 0.016f);

    int count = 0;
    for (auto e : reg.view<ObstacleTag>()) { ++count; (void)e; }
    CHECK(count == 0);
}

TEST_CASE("spawn: rhythm mode bypass even with expired timer", "[spawn][rhythm]") {
    auto reg = make_rhythm_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.spawn_timer = -5.0f;  // very expired

    obstacle_spawn_system(reg, 0.016f);

    int count = 0;
    for (auto e : reg.view<ObstacleTag>()) { ++count; (void)e; }
    CHECK(count == 0);
}

// ── obstacle_spawn_system: component validation ──────────────

TEST_CASE("spawn: ShapeGate has RequiredShape component", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.elapsed = 0.0f;  // Only ShapeGate at t=0
    config.spawn_timer = 0.0f;

    obstacle_spawn_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Obstacle, RequiredShape>();
    int count = 0;
    for (auto [e, obs, req] : view.each()) {
        CHECK(obs.kind == ObstacleKind::ShapeGate);
        CHECK(static_cast<int>(req.shape) >= 0);
        CHECK(static_cast<int>(req.shape) <= 2);  // Circle, Square, Triangle
        ++count;
    }
    CHECK(count == 1);
}

TEST_CASE("spawn: ShapeGate has Color component", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.elapsed = 0.0f;
    config.spawn_timer = 0.0f;

    obstacle_spawn_system(reg, 0.016f);

    auto view = reg.view<ObstacleTag, Color>();
    int count = 0;
    for (auto e : view) { ++count; (void)e; }
    CHECK(count == 1);
}

TEST_CASE("spawn: LanePushLeft or LanePushRight obstacles spawn at late game", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.elapsed = 35.0f;  // LaneBlock available

    for (int i = 0; i < 200; ++i) {
        config.spawn_timer = 0.0f;
        obstacle_spawn_system(reg, 0.016f);
    }

    bool found = false;
    auto view = reg.view<ObstacleTag, Obstacle>();
    for (auto [e, obs] : view.each()) {
        if (obs.kind == ObstacleKind::LanePushLeft || obs.kind == ObstacleKind::LanePushRight) {
            found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("spawn: LowBar has RequiredVAction component", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.elapsed = 50.0f;  // LowBar available

    for (int i = 0; i < 300; ++i) {
        config.spawn_timer = 0.0f;
        obstacle_spawn_system(reg, 0.016f);
    }

    bool found = false;
    auto view = reg.view<ObstacleTag, Obstacle, RequiredVAction>();
    for (auto [e, obs, vact] : view.each()) {
        if (obs.kind == ObstacleKind::LowBar) {
            CHECK(vact.action == VMode::Jumping);
            found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("spawn: HighBar has RequiredVAction for Sliding", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.elapsed = 65.0f;  // HighBar available

    for (int i = 0; i < 400; ++i) {
        config.spawn_timer = 0.0f;
        obstacle_spawn_system(reg, 0.016f);
    }

    bool found = false;
    auto view = reg.view<ObstacleTag, Obstacle, RequiredVAction>();
    for (auto [e, obs, vact] : view.each()) {
        if (obs.kind == ObstacleKind::HighBar) {
            CHECK(vact.action == VMode::Sliding);
            found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("spawn: ComboGate has both RequiredShape and BlockedLanes", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.elapsed = 95.0f;  // ComboGate available

    for (int i = 0; i < 500; ++i) {
        config.spawn_timer = 0.0f;
        obstacle_spawn_system(reg, 0.016f);
    }

    bool found = false;
    auto view = reg.view<ObstacleTag, Obstacle, RequiredShape, BlockedLanes>();
    for (auto [e, obs, req, blocked] : view.each()) {
        if (obs.kind == ObstacleKind::ComboGate) {
            found = true;
        }
    }
    CHECK(found);
}

TEST_CASE("spawn: SplitPath has RequiredShape and RequiredLane", "[spawn]") {
    auto reg = make_registry();
    auto& config = reg.ctx().get<DifficultyConfig>();
    config.elapsed = 125.0f;  // SplitPath available

    for (int i = 0; i < 600; ++i) {
        config.spawn_timer = 0.0f;
        obstacle_spawn_system(reg, 0.016f);
    }

    bool found = false;
    auto view = reg.view<ObstacleTag, Obstacle, RequiredShape, RequiredLane>();
    for (auto [e, obs, req, rlane] : view.each()) {
        if (obs.kind == ObstacleKind::SplitPath) {
            CHECK(rlane.lane >= 0);
            CHECK(rlane.lane <= 2);
            found = true;
        }
    }
    CHECK(found);
}
