#include "all_systems.h"
#include "../util/session_logger.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"

// Observes SongState::current_beat and emits BEAT log entries.
// Keeps last_logged_beat in SessionLog so playback has no logging branch.
void beat_log_system(entt::registry& reg, float /*dt*/) {
    auto* log = reg.ctx().find<SessionLog>();
    if (!log || !log->file) return;

    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto* song = reg.ctx().find<SongState>();
    if (!song || !song->playing || song->finished) return;

    if (song->current_beat > log->last_logged_beat) {
        for (int b = log->last_logged_beat + 1; b <= song->current_beat; ++b) {
            float expected = song->offset + b * song->beat_period;
            session_log_write(*log, song->song_time, "GAME",
                "BEAT %d expected=%.3f", b, expected);
        }
        log->last_logged_beat = song->current_beat;
    }
}
