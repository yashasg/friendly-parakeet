// Signal lifecycle tests for on_construct<ObstacleTag> — issue #342.
//
// Moves coverage of the ObstacleTag signal path out of any PLATFORM_DESKTOP
// guard so that native CI (Linux, macOS, Windows) sees these paths.
//
// Verifies:
//   1. on_construct<ObstacleTag> fires and increments ObstacleCounter.
//   2. on_destroy<ObstacleTag> fires and decrements ObstacleCounter.
//   3. wire_obstacle_counter is idempotent — calling it twice does not
//      double-connect the handler (no double-count).
//   4. Stale-signal safety: destroying all obstacles brings counter back to 0
//      without underflow.
//
// No PLATFORM_DESKTOP guard — pure EnTT/ECS unit tests.

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>
#include "test_helpers.h"
#include "systems/obstacle_counter_system.h"

// ── 1. on_construct fires and increments counter ─────────────────────────────

TEST_CASE("signal_lifecycle: on_construct<ObstacleTag> increments ObstacleCounter",
          "[signal][lifecycle][issue342]") {
    auto reg = make_registry();  // wire_obstacle_counter already called inside

    const auto& oc = reg.ctx().get<ObstacleCounter>();
    REQUIRE(oc.count == 0);

    auto e1 = reg.create();
    reg.emplace<ObstacleTag>(e1);
    CHECK(oc.count == 1);

    auto e2 = reg.create();
    reg.emplace<ObstacleTag>(e2);
    CHECK(oc.count == 2);
}

// ── 2. on_destroy fires and decrements counter ───────────────────────────────

TEST_CASE("signal_lifecycle: on_destroy<ObstacleTag> decrements ObstacleCounter",
          "[signal][lifecycle][issue342]") {
    auto reg = make_registry();
    const auto& oc = reg.ctx().get<ObstacleCounter>();

    auto e1 = reg.create();
    reg.emplace<ObstacleTag>(e1);
    auto e2 = reg.create();
    reg.emplace<ObstacleTag>(e2);
    REQUIRE(oc.count == 2);

    reg.destroy(e1);
    CHECK(oc.count == 1);

    reg.destroy(e2);
    CHECK(oc.count == 0);
}

// ── 3. No double-connect: wire_obstacle_counter called twice ─────────────────

TEST_CASE("signal_lifecycle: wire_obstacle_counter is idempotent (no double-count)",
          "[signal][lifecycle][issue342]") {
    auto reg = make_registry();  // first wire inside make_registry

    // Call again — should be a no-op because oc.wired == true.
    wire_obstacle_counter(reg);
    wire_obstacle_counter(reg);

    const auto& oc = reg.ctx().get<ObstacleCounter>();
    REQUIRE(oc.count == 0);

    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);

    // If double-connected, count would be 2 or 3 here.
    CHECK(oc.count == 1);
}

// ── 4. Counter returns to 0 after all obstacles destroyed ────────────────────

TEST_CASE("signal_lifecycle: counter returns to zero after all obstacles destroyed",
          "[signal][lifecycle][issue342]") {
    auto reg = make_registry();
    const auto& oc = reg.ctx().get<ObstacleCounter>();

    static constexpr int N = 10;
    entt::entity obs[N];
    for (int i = 0; i < N; ++i) {
        obs[i] = reg.create();
        reg.emplace<ObstacleTag>(obs[i]);
    }
    REQUIRE(oc.count == N);

    for (int i = 0; i < N; ++i)
        reg.destroy(obs[i]);

    CHECK(oc.count == 0);
}

// ── 5. Counter never goes negative on spurious destroy ───────────────────────

TEST_CASE("signal_lifecycle: counter does not go negative on excess destroy",
          "[signal][lifecycle][issue342]") {
    // Use a registry without make_registry to test the guard directly.
    entt::registry reg;
    reg.storage<ObstacleChildren>();
    reg.ctx().emplace<ObstacleCounter>();
    wire_obstacle_counter(reg);

    const auto& oc = reg.ctx().get<ObstacleCounter>();

    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);
    REQUIRE(oc.count == 1);

    reg.destroy(e);
    REQUIRE(oc.count == 0);

    // Manually drive count to 0 then destroy another — handler guards against underflow.
    auto e2 = reg.create();
    reg.emplace<ObstacleTag>(e2);
    reg.ctx().get<ObstacleCounter>().count = 0;  // artificially zero it
    reg.destroy(e2);

    CHECK(oc.count == 0);  // must not wrap negative
}

// ── 6. Wired flag prevents re-entry after manual reset ───────────────────────

TEST_CASE("signal_lifecycle: wired flag blocks reconnection after it is set",
          "[signal][lifecycle][issue342]") {
    auto reg = make_registry();
    auto& oc = reg.ctx().get<ObstacleCounter>();
    REQUIRE(oc.wired);

    // Even if someone forgets and calls wire again, wired flag blocks it.
    wire_obstacle_counter(reg);
    CHECK(oc.wired);

    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);
    CHECK(oc.count == 1);  // exactly once, not twice
}
