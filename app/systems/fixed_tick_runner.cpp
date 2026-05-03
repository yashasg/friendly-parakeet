#include "all_systems.h"

// One fixed timestep: game_state → song_playback → all Playing systems →
// obstacle lifecycle → UI feedback → particles.
// Extracted from game_loop.cpp so integration tests can call it directly
// without pulling in the full render/input/audio graph.
void tick_fixed_systems(entt::registry& reg, float dt) {
    // game_state_system runs FIRST and owns the authoritative GoEvent /
    // ButtonPressEvent drain for this tick (calls disp.update<GoEvent>() and
    // disp.update<ButtonPressEvent>() at its top).  All pre-tick enqueues from
    // input_system and gesture_routing are delivered
    // here to listeners in registration order (see wire_input_dispatcher).
    // Systems later in this list that also call disp.update<T>() (e.g.,
    // player_input_system) will find an empty queue and execute as no-ops.
    game_state_system(reg, dt);
    song_playback_system(reg, dt);
    tick_playing_systems(reg, dt);
    // Keep obstacle lifecycle systems contiguous while obstacle component pools
    // are still hot in cache from scroll/collision/miss/scoring passes.
    obstacle_despawn_system(reg, dt);
    // Keep score-feedback chain contiguous (queue -> popup spawn -> popup state)
    // while popup/score pools are warm.  popup_feedback and energy run here —
    // AFTER obstacle_despawn — so popup spawning observes post-despawn world
    // state (invariant: scoring settled, dead obstacles cleared, then feedback).
    // DO NOT move popup_feedback_system or energy_system into tick_playing_systems;
    // doing so reverses this ordering (see Keyser-r10 order-regression audit).
    popup_feedback_system(reg, dt);
    popup_display_system(reg, dt);
    energy_system(reg, dt);
    particle_system(reg, dt);
}
