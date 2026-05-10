#include "game_phase_transition.h"

void enter_phase(GameState& gs, GamePhase next_phase) {
    gs.previous_phase = gs.phase;
    gs.phase = next_phase;
    gs.phase_timer = 0.0f;
}
