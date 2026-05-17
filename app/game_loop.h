#pragma once
#include <cstdint>
#include <entt/entt.hpp>
#include "util/level_content_config.h"
#include "systems/test_player.h"

// Per-frame "we will not advance more than this" budget (seconds). The
// fixed-timestep accumulator and the variable-rate systems' raw_dt share
// this single canonical hitch cap so a window drag, debugger break,
// tab-inactive interval, or other main-thread hitch can never fast-forward
// touch-slot durations, AI action timers, or other dt-driven state
// (issue #1352).
inline constexpr float kMaxFrameDt = 0.1f;

// Clamp a raylib GetFrameTime() reading so a single hitch advances dt-driven
// state by at most kMaxFrameDt in one frame. Called once per frame from
// game_loop_frame; exported so tests can pin the contract without raylib init.
inline constexpr float clamp_frame_dt(float raw_dt) noexcept {
    return raw_dt < kMaxFrameDt ? raw_dt : kMaxFrameDt;
}

// Initialize platform (window, audio), all game singletons, cameras, meshes, UI.
// Returns false when platform initialization fails and the run loop must not start.
bool game_loop_init(entt::registry& reg,
                    bool test_player_mode = false,
                    SkillConfig test_skill = SKILL_PRO,
                    const char* difficulty = "medium",
                    int selected_level = content_config::DEFAULT_LEVEL_INDEX);

// Run the game loop (blocks until quit). Handles Emscripten vs native.
void game_loop_run(entt::registry& reg);

// Single frame tick (used by Emscripten main loop callback).
void game_loop_frame(entt::registry& reg, float& accumulator);

// Shutdown: unload GPU resources, close logs, release audio, close window,
// and reset registry context/listeners so game_loop_init can be called again.
void game_loop_shutdown(entt::registry& reg);

// Returns true when the game loop should exit (quit requested or window closed).
// Used by both native and Emscripten paths to detect shutdown conditions.
bool game_loop_should_quit(entt::registry& reg);
