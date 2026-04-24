#pragma once

#include <entt/entt.hpp>

// Phase 0: Raw input (polls raylib input)
void input_system(entt::registry& reg, float raw_dt);

// Phase 0.25: Hit-test (resolves taps → button presses, swipes → go events)
void hit_test_system(entt::registry& reg);

// Phase 0.5: Test player AI (pushes synthetic input events into EventQueue)
void test_player_system(entt::registry& reg, float dt);

// Test player initialization (call once from main if --test-player is set)
enum class TestPlayerSkill : uint8_t;
void test_player_init(entt::registry& reg, TestPlayerSkill skill,
                      const char* difficulty);

// Phase 2: Game State
void game_state_system(entt::registry& reg, float dt);
void level_select_system(entt::registry& reg, float dt);

// Phase 3: Rhythm Engine
void song_playback_system(entt::registry& reg, float dt);
void beat_scheduler_system(entt::registry& reg, float dt);

// Phase 4: Player
void player_input_system(entt::registry& reg, float dt);
void shape_window_system(entt::registry& reg, float dt);
void player_movement_system(entt::registry& reg, float dt);

// Phase 5: World
void difficulty_system(entt::registry& reg, float dt);
void obstacle_spawn_system(entt::registry& reg, float dt);
void scroll_system(entt::registry& reg, float dt);
void ring_zone_log_system(entt::registry& reg, float dt);
void burnout_system(entt::registry& reg, float dt);
void collision_system(entt::registry& reg, float dt);
void scoring_system(entt::registry& reg, float dt);

// Phase 5.5: Energy
void energy_system(entt::registry& reg, float dt);

// Phase 6: Cleanup
void lifetime_system(entt::registry& reg, float dt);
void particle_system(entt::registry& reg, float dt);
void cleanup_system(entt::registry& reg, float dt);

// Phase 6.5: UI prep
void popup_display_system(entt::registry& reg, float dt);
void ui_navigation_system(entt::registry& reg, float dt);

// Phase 7: Camera
void game_camera_system(entt::registry& reg, float dt);  // model-to-world transforms
void ui_camera_system(entt::registry& reg, float dt);    // screen-space transforms

// Phase 8: Render — world pass (3D) + UI pass (2D)
void game_render_system(entt::registry& reg, float alpha);
void ui_render_system(entt::registry& reg, float alpha);

// Audio (no dt needed)
void audio_system(entt::registry& reg);
