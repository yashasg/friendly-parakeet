#include "all_systems.h"
#include "../components/game_state.h"

// Runs all systems that should execute only during the Playing phase.
// Single phase-gate at the runner boundary; individual systems carry no guard.
// Per Fabian's existential processing (#1202/#1204 PR F), the gate dispatches
// on the `GamePhasePlayingTag` ctx mirror instead of reading `gs.phase`.
void tick_playing_systems(entt::registry& reg, float dt) {
    if (!reg.ctx().contains<GamePhasePlayingTag>()) return;
    beat_log_system(reg, dt);
    beat_scheduler_system(reg, dt);
    shape_window_activation_system(reg, dt);
    player_movement_system(reg, dt);
    scroll_system(reg, dt);
    motion_system(reg, dt);
    collision_system(reg, dt);
    shape_window_system(reg, dt);
    miss_detection_system(reg, dt);
    scoring_system(reg, dt);
}
