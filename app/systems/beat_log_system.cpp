#include "all_systems.h"
#include "session_logger_system.h"
#include "../entities/beat_map.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../components/song_state.h"

// Observes BeatCursor and emits BEAT log entries.
// Keeps LastLoggedBeat in registry context so playback has no logging branch.
// Both singletons follow Fabian Principle 3 (issue #1545): membership IS the
// precondition that any beat has been crossed / emitted, so there is no
// `-1` sentinel discriminator in the read path.
void beat_log_system(entt::registry& reg, [[maybe_unused]] float dt) {
    auto& ctx = reg.ctx();
    auto* log = ctx.find<SessionLog>();
    if (!log || !log->file) return;

    auto* song = ctx.find<SongState>();
    if (!song || !song->playing || song->finished) return;

    const auto* cursor = ctx.find<BeatCursor>();
    if (!cursor) return;  // no beat crossed yet → nothing to log

    auto* last_logged = ctx.find<LastLoggedBeat>();
    const int last = last_logged ? last_logged->beat : -1;
    if (cursor->last_crossed <= last) return;

    auto* map = find_beat_map(reg);
    for (int b = last + 1; b <= cursor->last_crossed; ++b) {
        float expected = song->offset + b * song->beat_period;
        if (map && b >= 0 && static_cast<size_t>(b) < map->beat_times.size()) {
            expected = map->beat_times[static_cast<size_t>(b)];
        }
        session_log_write(*log, song->song_time, "GAME",
            "BEAT %d expected=%.3f", b, expected);
    }
    if (last_logged) {
        last_logged->beat = cursor->last_crossed;
    } else {
        ctx.emplace<LastLoggedBeat>(LastLoggedBeat{cursor->last_crossed});
    }
}
