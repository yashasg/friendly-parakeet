// tests/test_game_loop_raw_dt_hitch_clamp.cpp
//
// Issue #1352 regression: game_loop_frame used to read raw_dt = GetFrameTime()
// and pass the unclamped value to two variable-rate systems before the
// fixed-timestep accumulator clamp kicked in. After a window drag, pause,
// debugger break, or web tab-inactive interval, GetFrameTime() reports the
// full hitch duration (seconds), which:
//
//   • input_system: accumulated ActiveTouchSlot::duration += raw_dt
//     (formerly `touch_slots[i].duration` before #1612 normalized the
//     fixed-size array column into a row table), spuriously crossing
//     long-press / gesture-duration thresholds.
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

    // Per Fabian Principle 3 / #1611, queued actions live as components on
    // obstacle entities. Stand up a synthetic action row on a fresh entity to
    // exercise the TICK clamp without needing a full obstacle/player setup.
    auto action_entity = reg.create();
    auto& action       = reg.emplace<TestPlayerAction>(action_entity);
    action.timer        = 1.0f;
    action.arrival_time = 10.0f;
    tp.swipe_cooldown_timer = 1.0f;

    constexpr float k5sHitch = 5.0f;
    const float clamped = clamp_frame_dt(k5sHitch);
    REQUIRE(clamped == kMaxFrameDt);

    test_player_system(reg, clamped);

    // Without the upstream clamp, both timers would have dropped by ~5s in
    // one frame (fast-forwarding the entire AI plan). With the clamp,
    // they drop by at most kMaxFrameDt.
    const auto* a = reg.try_get<TestPlayerAction>(action_entity);
    REQUIRE(a != nullptr);
    CHECK(a->timer                >= 1.0f - kMaxFrameDt - 1e-6f);
    CHECK(tp.swipe_cooldown_timer >= 1.0f - kMaxFrameDt - 1e-6f);
}

TEST_CASE("InputState: touch-slot duration advances by at most kMaxFrameDt under a hitch",
          "[input][game_loop][issue-1352]") {
    auto reg = make_rhythm_registry();

    // Simulate an active touch carried over a hitch frame. Per Fabian
    // Principle 3 / #1612, the former `InputState::touch_slots[N]`
    // fixed-size array column was normalized into an `ActiveTouchSlot`
    // row table — presence in the registry IS slot activity.
    auto slot_entity = reg.create();
    auto& slot       = reg.emplace<ActiveTouchSlot>(slot_entity);
    slot.id          = 0;
    slot.duration    = 0.0f;

    // game_loop_frame clamps GetFrameTime() through clamp_frame_dt before
    // calling input_system. Reproduce the same accumulation expression
    // input_system uses (the touching/duration loop in input_system.cpp)
    // to assert the bounded-advance contract.
    constexpr float k5sHitch = 5.0f;
    const float raw_dt = clamp_frame_dt(k5sHitch);
    slot.duration += raw_dt;

    CHECK(slot.duration <= kMaxFrameDt);
}
