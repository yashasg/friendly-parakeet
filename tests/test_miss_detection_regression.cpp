// Regression tests for miss_detection_system — issue #336.
//
// Verifies:
//   1. N expired obstacles (past DESTROY_Y) each receive exactly one MissTag
//      and one ScoredTag on the first pass.
//   2. Idempotence: a second call to miss_detection_system adds no new tags
//      (the exclude<ScoredTag> view correctly skips already-tagged obstacles).
//
// No PLATFORM_DESKTOP guard — these are pure ECS unit tests.

#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

// ── helpers ─────────────────────────────────────────────────────────────────

// Create a minimal expired obstacle: has ObstacleTag + Obstacle + Position past
// DESTROY_Y, but no ScoredTag (miss_detection_system's precondition).
static entt::entity make_expired_obstacle(entt::registry& reg) {
    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);
    reg.emplace<Obstacle>(e, ObstacleKind::ShapeGate,
                          static_cast<int16_t>(constants::PTS_SHAPE_GATE));
    reg.emplace<Position>(e, 0.0f, constants::DESTROY_Y + 10.0f);
    return e;
}

// Count entities in reg that carry both MissTag and ScoredTag.
static int count_miss_scored(entt::registry& reg) {
    int n = 0;
    for (auto e : reg.view<MissTag, ScoredTag>()) {
        (void)e;
        ++n;
    }
    return n;
}

// ── #336: N expired → exactly N MissTag + ScoredTag ─────────────────────────

TEST_CASE("miss_detection: N expired obstacles each receive MissTag and ScoredTag",
          "[miss_detection][regression][issue336]") {
    static constexpr int N = 7;

    auto reg = make_registry();
    // GameState is Playing by default from make_registry.

    entt::entity obs[N];
    for (int i = 0; i < N; ++i)
        obs[i] = make_expired_obstacle(reg);

    miss_detection_system(reg, 0.016f);

    for (int i = 0; i < N; ++i) {
        INFO("obstacle index " << i);
        CHECK(reg.all_of<MissTag>(obs[i]));
        CHECK(reg.all_of<ScoredTag>(obs[i]));
    }
    CHECK(count_miss_scored(reg) == N);
}

// ── #336: idempotence ────────────────────────────────────────────────────────

TEST_CASE("miss_detection: second run adds no new MissTag or ScoredTag (idempotence)",
          "[miss_detection][regression][issue336]") {
    static constexpr int N = 5;

    auto reg = make_registry();

    for (int i = 0; i < N; ++i)
        make_expired_obstacle(reg);

    miss_detection_system(reg, 0.016f);

    // First run: all tagged.
    const int after_first = count_miss_scored(reg);
    REQUIRE(after_first == N);

    // Second run must not re-tag or double-emplace.
    miss_detection_system(reg, 0.016f);

    const int after_second = count_miss_scored(reg);
    CHECK(after_second == N);

    // Ensure no entity gained a duplicate MissTag (EnTT ignores second emplace but
    // we validate the count didn't grow, confirming exclude<ScoredTag> gates correctly).
    int miss_only = 0;
    for (auto e : reg.view<MissTag>()) { (void)e; ++miss_only; }
    int scored_only = 0;
    for (auto e : reg.view<ScoredTag>()) { (void)e; ++scored_only; }
    CHECK(miss_only == N);
    CHECK(scored_only == N);
}

// ── Non-expired obstacles are not tagged ────────────────────────────────────

TEST_CASE("miss_detection: obstacles above DESTROY_Y are not tagged",
          "[miss_detection][regression][issue336]") {
    auto reg = make_registry();

    // One expired obstacle that should be tagged.
    auto expired = make_expired_obstacle(reg);

    // One obstacle still on-screen that must not be tagged.
    auto active = reg.create();
    reg.emplace<ObstacleTag>(active);
    reg.emplace<Obstacle>(active, ObstacleKind::ShapeGate,
                          static_cast<int16_t>(constants::PTS_SHAPE_GATE));
    reg.emplace<Position>(active, 0.0f, constants::PLAYER_Y - 100.0f);

    miss_detection_system(reg, 0.016f);

    CHECK(reg.all_of<MissTag>(expired));
    CHECK(reg.all_of<ScoredTag>(expired));
    CHECK_FALSE(reg.all_of<MissTag>(active));
    CHECK_FALSE(reg.all_of<ScoredTag>(active));
}

// ── Already-ScoredTag obstacles are skipped ─────────────────────────────────

TEST_CASE("miss_detection: pre-scored obstacles are excluded from miss tagging",
          "[miss_detection][regression][issue336]") {
    auto reg = make_registry();

    // Obstacle already scored (e.g. by collision_system) — must not get MissTag.
    auto already_scored = make_expired_obstacle(reg);
    reg.emplace<ScoredTag>(already_scored);

    // Unscored expired obstacle — must get MissTag.
    auto unscored = make_expired_obstacle(reg);

    miss_detection_system(reg, 0.016f);

    CHECK_FALSE(reg.all_of<MissTag>(already_scored));
    CHECK(reg.all_of<MissTag>(unscored));
    CHECK(reg.all_of<ScoredTag>(unscored));
}

// ── Non-Playing phase: system is a no-op ────────────────────────────────────

TEST_CASE("miss_detection: no-op when game phase is not Playing",
          "[miss_detection][regression][issue336]") {
    auto reg = make_registry();
    reg.ctx().get<GameState>().phase = GamePhase::GameOver;

    auto obs = make_expired_obstacle(reg);

    miss_detection_system(reg, 0.016f);

    CHECK_FALSE(reg.all_of<MissTag>(obs));
    CHECK_FALSE(reg.all_of<ScoredTag>(obs));
}
