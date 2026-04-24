#pragma once

#include <entt/entt.hpp>

// Phase 0: Raw input (polls raylib input)
void input_system(entt::registry& reg, float raw_dt);

// Phase 0.25: Hit-test (resolves taps → button presses, swipes → go events)
void hit_test_system(entt::registry& reg);

// Phase 0.5: Test player AI (injects into InputState key flags)
void test_player_system(entt::registry& reg, float dt);

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

// Phase 6.5: UI prep (compute popup display data from scoring + lifetime)
void popup_display_system(entt::registry& reg, float dt);

// Phase 7: Camera (updates screen transform + model-to-world matrices)
void camera_system(entt::registry& reg, float dt);

// Phase 8: Render — world pass (3D) + UI pass (2D)
void render_world_system(entt::registry& reg, float alpha);
void render_ui_system(entt::registry& reg, float alpha);

// Audio (no dt needed)
void audio_system(entt::registry& reg);
