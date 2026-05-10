#pragma once

#include <entt/entt.hpp>
#include "../components/game_state.h"

// Phase 0.5: Test player AI (enqueues synthetic input actions)
void runtime_system_scratch_init(entt::registry& reg);
void test_player_system(entt::registry& reg, float dt);

// Phase 2: Game State
void game_state_system(entt::registry& reg, float dt);
void game_state_enter_terminal_phase(entt::registry& reg, GamePhase phase);
void game_state_end_screen_system(entt::registry& reg, float dt);

// Phase 3: Rhythm Engine (headless)
void beat_log_system(entt::registry& reg, float dt);
void beat_scheduler_system(entt::registry& reg, float dt);

// Phase 4: Player
void shape_window_system(entt::registry& reg, float dt);
void player_movement_system(entt::registry& reg, float dt);

// Phase 5: World
void scroll_system(entt::registry& reg, float dt);
void motion_system(entt::registry& reg, float dt);
void collision_system(entt::registry& reg, float dt);
void miss_detection_system(entt::registry& reg, float dt);
void scoring_system(entt::registry& reg, float dt);
void popup_feedback_system(entt::registry& reg, float dt);

// Phase 5.5: Energy
void energy_system(entt::registry& reg, float dt);

// Phase runner: all Playing-only systems behind a single phase gate.
// Call from tick_fixed_systems in place of the 13 individual Playing-gated calls.
void tick_playing_systems(entt::registry& reg, float dt);

// Phase 6: Cleanup
void particle_system(entt::registry& reg, float dt);
// Destroys obstacle entities that have scrolled past the camera's far-Z boundary.
void obstacle_despawn_system(entt::registry& reg, float dt);

// Phase 6.5: UI prep
void popup_display_system(entt::registry& reg, float dt);

// Haptics (no dt needed) — drains PlayHapticEvent via dispatcher; settings gating is in listener
void haptic_system(entt::registry& reg);
