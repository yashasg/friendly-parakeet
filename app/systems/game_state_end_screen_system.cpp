#include "all_systems.h"
#include "../components/game_state.h"

namespace {

GamePhase resolve_end_screen_phase(EndScreenChoice choice) {
    if (choice == EndScreenChoice::Restart) return GamePhase::Playing;
    if (choice == EndScreenChoice::LevelSelect) return GamePhase::LevelSelect;
    return GamePhase::Title;
}

}  // namespace

void game_state_end_screen_system(entt::registry& reg, float /*dt*/) {
    auto& gs = reg.ctx().get<GameState>();
    const bool game_over_ready =
        gs.phase == GamePhase::GameOver && gs.phase_timer > 0.4f;
    const bool song_complete_ready =
        gs.phase == GamePhase::SongComplete && gs.phase_timer > 0.5f;
    if (!(game_over_ready || song_complete_ready) ||
        gs.end_choice == EndScreenChoice::None) {
        return;
    }

    gs.transition_pending = true;
    gs.next_phase = resolve_end_screen_phase(gs.end_choice);
    gs.end_choice = EndScreenChoice::None;
}
