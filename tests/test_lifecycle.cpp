// Tests for game_loop_should_quit — the shared quit-condition predicate
// used by both the native run-loop and the Emscripten frame callback.
//
// Full end-to-end WASM lifecycle (browser events, beforeunload, main loop
// cancel) cannot be exercised in this native test binary. Those paths are
// validated by compile-only WASM CI builds (ci-wasm.yml). This file covers
// the logic that IS portable: the quit-detection predicate and its use of
// the InputState context singleton.
#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>
#include "components/input.h"
#include "game_loop.h"

TEST_CASE("lifecycle: game_loop_should_quit returns false with no InputState", "[lifecycle]") {
    entt::registry reg;
    // No InputState in context — should not crash and should return false.
    CHECK_FALSE(game_loop_should_quit(reg));
}

TEST_CASE("lifecycle: game_loop_should_quit returns false when quit not requested", "[lifecycle]") {
    entt::registry reg;
    reg.ctx().emplace<InputState>();
    CHECK_FALSE(game_loop_should_quit(reg));
}

TEST_CASE("lifecycle: game_loop_should_quit returns true when quit_requested", "[lifecycle]") {
    entt::registry reg;
    auto& input = reg.ctx().emplace<InputState>();
    input.quit_requested = true;
    CHECK(game_loop_should_quit(reg));
}

TEST_CASE("lifecycle: game_loop_should_quit resets to false after clearing quit flag", "[lifecycle]") {
    entt::registry reg;
    auto& input = reg.ctx().emplace<InputState>();
    input.quit_requested = true;
    CHECK(game_loop_should_quit(reg));
    input.quit_requested = false;
    CHECK_FALSE(game_loop_should_quit(reg));
}
