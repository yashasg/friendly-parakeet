#include "all_systems.h"

// One fixed timestep: game_state → song_playback → all Playing systems →
// obstacle lifecycle → UI feedback → particles.
// Extracted from game_loop.cpp so integration tests can call it directly
// without pulling in the full render/input/audio graph.
void tick_fixed_systems(entt::registry& reg, float dt) {
    // game_state_system runs FIRST and owns the authoritative GoEvent /
    // ButtonPressEvent drain for this tick (via drain_semantic_input_events()).
    // All pre-tick enqueues from input_system and swipe routing are delivered
    // here to listeners in registration order (see wire_input_dispatcher).
    // Systems later in this list consume state produced by those listeners.
    game_state_system(reg, dt);
    song_playback_system(reg, dt);
    tick_playing_systems(reg, dt);
    // Keep obstacle lifecycle systems contiguous while obstacle component pools
    // are still hot in cache from scroll/collision/miss/scoring passes.
    obstacle_despawn_system(reg, dt);
    // Keep score-feedback chain contiguous (queue -> popup spawn -> popup state)
    // while popup/score pools are warm.  popup_feedback and energy run here
    // after obstacle_despawn as a cache-locality preference, NOT as a semantic
    // invariant.  These two systems are commutative: obstacle_despawn reads
    // ObstacleTag+ObstacleScrollZ and destroys entities; popup_feedback
    // reads ScorePopupRequestQueue (a ctx variable populated by scoring_system
    // inside tick_playing_systems) and creates popup entities.  Their data
    // surfaces are disjoint — swapping them produces no observable state diff
    // (Keaton-r14 analysis).  DO NOT move popup_feedback_system or energy_system
    // INTO tick_playing_systems; that would run them before scoring_system
    // populates the queue, silently dropping all popups.
    //
    // Note: the obstacle_despawn read set is ObstacleTag+ObstacleScrollZ
    // (Position component was deleted in r16 — see decisions.md Round 16).
    popup_feedback_system(reg, dt);
    popup_display_system(reg, dt);
    energy_system(reg, dt);
    particle_system(reg, dt);
}
