#pragma once
#include <entt/entt.hpp>

// Forward-declare to avoid including test_player.h in the header.
enum class TestPlayerSkill : uint8_t;

// Initialize platform (window, audio), all game singletons, cameras, meshes, UI.
void game_loop_init(entt::registry& reg,
                    bool test_player_mode = false,
                    TestPlayerSkill test_skill = {},
                    const char* difficulty = "medium");

// Run the game loop (blocks until quit). Handles Emscripten vs native.
void game_loop_run(entt::registry& reg);

// Shutdown: unload GPU resources, close logs, release audio, close window.
void game_loop_shutdown(entt::registry& reg);
