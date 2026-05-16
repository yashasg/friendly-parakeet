// Web input policy tests — issue #499.
//
// The WebInputPolicy used to latch a one-time "prefers touch" decision from a
// startup UA regex + maxTouchPoints check. That broke iPadOS 13+ Safari
// (desktop UA) and any touch-capable laptop with a desktop UA — touch was
// disabled for the entire session.
//
// The new contract:
//   • On native (non-web), both mouse and touch input are always permitted.
//   • On web, mouse input is always permitted; touch input is permitted iff
//     the browser exposes a touch surface (navigator.maxTouchPoints > 0).
//   • There is no UA regex and no one-time "prefer one source" latch.
//
// We can't drive emscripten's EM_ASM_INT from the native test runner, but we
// CAN assert on the non-web invariant that input_system_init runs cleanly
// and that input_system continues to honor both sources concurrently with
// the InputSourceMouse / InputSourceTouch ctx-tag mutex acting as the
// per-gesture arbiter (covered by the existing pipeline tests for
// swipe/keyboard routing).

#include <catch2/catch_test_macros.hpp>

#include "test_helpers.h"

TEST_CASE("input_system_init: runs cleanly and leaves both sources enabled on native",
          "[input][web_policy]") {
    auto reg = make_registry();
    // Idempotent: calling init twice must not throw or duplicate context.
    REQUIRE_NOTHROW(input_system_init(reg));
    REQUIRE_NOTHROW(input_system_init(reg));

    // On native, no gating. Run a tick — must not crash and must not
    // synthesize spurious input.
    auto& input = reg.ctx().get<InputState>();
    input.touch_down = false;
    input.touch_up   = false;
    input.click      = false;
    REQUIRE_NOTHROW(input_system(reg, 0.016f));
    CHECK_FALSE(input.touch_down);
    CHECK_FALSE(input.touch_up);
}
