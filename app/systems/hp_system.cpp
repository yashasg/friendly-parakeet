#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"

void hp_system(entt::registry& reg, float /*dt*/) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto* hp = reg.ctx().find<HPState>();
    if (!hp) return;

    auto* song = reg.ctx().find<SongState>();
    if (!song || !song->playing) return;

    if (hp->current <= 0) {
        auto& gs = reg.ctx().get<GameState>();
        gs.transition_pending = true;
        gs.next_phase = GamePhase::GameOver;
        song->finished = true;
        song->playing  = false;
    }
}
