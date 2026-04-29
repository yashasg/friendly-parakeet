#pragma once

#include <entt/entt.hpp>
#include "../components/game_state.h"

// Phase 0: Raw input (polls raylib input)
void input_system(entt::registry& reg, float raw_dt);

// Phase 0.5: Test player AI (enqueues synthetic input actions)
void test_player_system(entt::registry& reg, float dt);

// Phase 2: Game State
void game_state_system(entt::registry& reg, float dt);
void game_state_enter_terminal_phase(entt::registry& reg, GamePhase phase);
void game_state_end_screen_system(entt::registry& reg, float dt);

// Phase 3: Rhythm Engine
void song_playback_system(entt::registry& reg, float dt);
void beat_log_system(entt::registry& reg, float dt);
void beat_scheduler_system(entt::registry& reg, float dt);

// Phase 4: Player
void player_input_system(entt::registry& reg, float dt);
void shape_window_system(entt::registry& reg, float dt);
void player_movement_system(entt::registry& reg, float dt);

// Phase 5: World
void scroll_system(entt::registry& reg, float dt);
void collision_system(entt::registry& reg, float dt);
void miss_detection_system(entt::registry& reg, float dt);
void scoring_system(entt::registry& reg, float dt);
void popup_feedback_system(entt::registry& reg, float dt);

// Phase 5.5: Energy
void energy_system(entt::registry& reg, float dt);

// Phase 6: Cleanup
void particle_system(entt::registry& reg, float dt);
// Destroys obstacle entities that have scrolled past the camera's far-Z boundary.
void obstacle_despawn_system(entt::registry& reg, float dt);

// Phase 6.5: UI prep
void popup_display_system(entt::registry& reg, float dt);
void ui_navigation_system(entt::registry& reg, float dt);

// Phase 7: Camera
void game_camera_system(entt::registry& reg, float dt);  // model-to-world transforms
void ui_camera_system(entt::registry& reg, float dt);    // screen-space transforms

// Phase 8: Render — world pass (3D) + UI pass (2D)
void game_render_system(const entt::registry& reg, float alpha);
void ui_render_system(entt::registry& reg, float alpha);

void audio_system(entt::registry& reg);

// Haptics (no dt needed) — drains HapticQueue; gating is at push time via haptics_enabled
void haptic_system(entt::registry& reg);
