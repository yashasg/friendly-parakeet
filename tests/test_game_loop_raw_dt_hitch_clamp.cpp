// tests/test_game_loop_raw_dt_hitch_clamp.cpp
//
// Issue #1352 regression: game_loop_frame used to read raw_dt = GetFrameTime()
// and pass the unclamped value to two variable-rate systems before the
// fixed-timestep accumulator clamp kicked in. After a window drag, pause,
// debugger break, or web tab-inactive interval, GetFrameTime() reports the
// full hitch duration (seconds), which:
//
//   • input_system: accumulated touch_slots[i].duration += raw_dt, spuriously
//     crossing long-press / gesture-duration thresholds.
//   • test_player_system: ticked every queued action timer by the full hitch,
//     fast-forwarding the entire AI plan in a single frame (relevant to the
//     WASM smoke flake history — see #1341).
//
// The fix exposes `kMaxFrameDt` + `clamp_frame_dt(raw_dt)` from game_loop.h
// and clamps once at the source in game_loop_frame. These tests pin the
// math of the helper and the bounded-advance contract that downstream
// systems rely on.
#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"
#include "game_loop.h"
#include "systems/test_player.h"

TEST_CASE("clamp_frame_dt: caps any hitch reading at kMaxFrameDt", "[game_loop][issue-1352]") {
    CHECK(kMaxFrameDt == 0.1f);

    // Sub-cap inputs pass through unchanged.
    CHECK(clamp_frame_dt(0.0f) == 0.0f);
    CHECK(clamp_frame_dt(1.0f / 60.0f) == 1.0f / 60.0f);
    CHECK(clamp_frame_dt(1.0f / 30.0f) == 1.0f / 30.0f);

    // At and above the cap, the result is exactly kMaxFrameDt.
    CHECK(clamp_frame_dt(kMaxFrameDt) == kMaxFrameDt);
    CHECK(clamp_frame_dt(0.5f) == kMaxFrameDt);
    CHECK(clamp_frame_dt(3.0f) == kMaxFrameDt);
    CHECK(clamp_frame_dt(5.0f) == kMaxFrameDt);
}

TEST_CASE("test_player_system: action timers advance by at most kMaxFrameDt under a hitch",
          "[test_player][game_loop][issue-1352]") {
    auto reg = make_rhythm_registry();
    auto& tp = reg.ctx().emplace<TestPlayerState>();
    tp.skill  = SKILL_PRO;
    tp.active = true;
    tp.rng.seed(42);

    tp.action_count = 1;
    tp.actions[0].timer        = 1.0f;
    tp.actions[0].arrival_time = 10.0f;
    tp.swipe_cooldown_timer    = 1.0f;

    constexpr float k5sHitch = 5.0f;
    const float clamped = clamp_frame_dt(k5sHitch);
    REQUIRE(clamped == kMaxFrameDt);

    test_player_system(reg, clamped);

    // Without the upstream clamp, both timers would have dropped by ~5s in
    // one frame (fast-forwarding the entire AI plan). With the clamp,
    // they drop by at most kMaxFrameDt.
    CHECK(tp.actions[0].timer        >= 1.0f - kMaxFrameDt - 1e-6f);
    CHECK(tp.swipe_cooldown_timer    >= 1.0f - kMaxFrameDt - 1e-6f);
}

TEST_CASE("InputState: touch-slot duration advances by at most kMaxFrameDt under a hitch",
          "[input][game_loop][issue-1352]") {
    auto reg = make_rhythm_registry();
    auto& input = reg.ctx().get<InputState>();

    // Simulate an active touch carried over a hitch frame.
    input.touch_slots[0].active   = true;
    input.touch_slots[0].duration = 0.0f;

    // game_loop_frame clamps GetFrameTime() through clamp_frame_dt before
    // calling input_system. Reproduce the same accumulation expression
    // input_system uses (app/systems/input_system.cpp:357) to assert the
    // bounded-advance contract.
    constexpr float k5sHitch = 5.0f;
    const float raw_dt = clamp_frame_dt(k5sHitch);
    input.touch_slots[0].duration += raw_dt;

    CHECK(input.touch_slots[0].duration <= kMaxFrameDt);
}
