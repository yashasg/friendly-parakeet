#pragma once
#include <cstdint>
#include <entt/entt.hpp>
#include "util/level_content_config.h"
#include "systems/test_player.h"

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
