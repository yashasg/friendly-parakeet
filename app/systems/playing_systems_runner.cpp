#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/registry_context.h"

// Runs all systems that should execute only during GamePhase::Playing.
// Single phase-gate at the runner boundary; individual systems carry no guard.
void tick_playing_systems(entt::registry& reg, float dt) {
    const auto* game_state = registry_ctx_find<GameState>(reg);
    if (!game_state || !is_playing_phase(game_state->phase)) return;
    beat_log_system(reg, dt);
    beat_scheduler_system(reg, dt);
    shape_window_system(reg, dt);
    player_movement_system(reg, dt);
    scroll_system(reg, dt);
    motion_system(reg, dt);
    collision_system(reg, dt);
    miss_detection_system(reg, dt);
    scoring_system(reg, dt);
}
