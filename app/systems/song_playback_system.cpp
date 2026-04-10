#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"

void song_playback_system(entt::registry& reg, float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto* song = reg.ctx().find<SongState>();
    if (!song || !song->playing || song->finished) return;

    song->song_time += dt;

    // Update current beat
    if (song->beat_period > 0.0f) {
        int beat = static_cast<int>((song->song_time - song->offset) / song->beat_period);
        if (beat > song->current_beat && song->song_time >= song->offset) {
            song->current_beat = beat;
        }
    }

    // Detect song end
    if (song->song_time >= song->duration_sec) {
        song->finished = true;
        song->playing  = false;
    }
}
