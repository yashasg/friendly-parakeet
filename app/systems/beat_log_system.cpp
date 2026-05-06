#include "all_systems.h"
#include "../util/session_logger.h"
#include "../components/rhythm.h"
#include "../components/registry_context.h"

namespace {

float expected_beat_time(const SongState& song, const BeatMap* map, int beat_index) {
    if (map && beat_index >= 0 &&
        static_cast<size_t>(beat_index) < map->beat_times.size()) {
        return map->beat_times[static_cast<size_t>(beat_index)];
    }
    return song.offset + beat_index * song.beat_period;
}

}  // namespace

// Observes SongState::current_beat and emits BEAT log entries.
// Keeps last_logged_beat in SessionLog so playback has no logging branch.
void beat_log_system(entt::registry& reg, float /*dt*/) {
    auto* log = registry_ctx_find<SessionLog>(reg);
    if (!log || !log->file) return;

    auto* song = registry_ctx_find<SongState>(reg);
    auto* map = registry_ctx_find<BeatMap>(reg);
    if (!song || !song->playing || song->finished) return;

    if (song->current_beat > log->last_logged_beat) {
        for (int b = log->last_logged_beat + 1; b <= song->current_beat; ++b) {
            const float expected = expected_beat_time(*song, map, b);
            session_log_write(*log, song->song_time, "GAME",
                "BEAT %d expected=%.3f", b, expected);
        }
        log->last_logged_beat = song->current_beat;
    }
}
