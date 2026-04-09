#pragma once

#include <entt/entt.hpp>

// Phase 1: Input
void gesture_system(entt::registry& reg, float dt);

// Phase 2: Game State
void game_state_system(entt::registry& reg, float dt);

// Phase 3: Player
void player_action_system(entt::registry& reg, float dt);
void player_movement_system(entt::registry& reg, float dt);

// Phase 4: World
void difficulty_system(entt::registry& reg, float dt);
void obstacle_spawn_system(entt::registry& reg, float dt);
void scroll_system(entt::registry& reg, float dt);
void burnout_system(entt::registry& reg, float dt);
void collision_system(entt::registry& reg, float dt);
void scoring_system(entt::registry& reg, float dt);

// Phase 5: Cleanup
void lifetime_system(entt::registry& reg, float dt);
void particle_system(entt::registry& reg, float dt);
void cleanup_system(entt::registry& reg, float dt);

// Phase 6: Render (takes renderer + interpolation alpha)
struct SDL_Renderer;
void render_system(entt::registry& reg, SDL_Renderer* renderer, float alpha);

// Audio (no dt needed)
void audio_system(entt::registry& reg);
