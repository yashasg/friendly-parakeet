#include "all_systems.h"
#include "runtime_systems.h"

// Fixed-timestep cycle extracted for tests without the render/input/audio graph.
void tick_fixed_systems(entt::registry& reg, float dt) {
    // game_state_system runs first and owns the semantic input event drain.
    game_state_system(reg, dt);
    song_playback_system(reg, dt);
    tick_playing_systems(reg, dt);
    // Keep obstacle lifecycle systems contiguous while pools are warm.
    obstacle_despawn_system(reg, dt);
    // Score feedback runs after scoring emplaces PopupRequest row entities
    // (issue #1626 — per-tier row tables replaced the old ScorePopupRequestQueue).
    popup_feedback_system(reg, dt);
    popup_display_system(reg, dt);
    energy_system(reg, dt);
    energy_bar_system(reg, dt);
    particle_system(reg, dt);
}
