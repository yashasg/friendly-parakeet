#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/transform.h"
#include "../components/rhythm.h"
#include "../components/obstacle.h"
#include "../constants.h"

#include <cmath>

void scroll_system(entt::registry& reg, [[maybe_unused]] float dt) {
    auto* song = reg.ctx().find<SongState>();
    const auto& ctx = reg.ctx();

    // Rhythm obstacles: position derived from song_time, not accumulated dt.
    // This prevents floating-point drift from desynchronizing collisions
    // with the beat grid. (See: "never use anything other than song position")
    if (song && (ctx.contains<SongPlayingTag>() || ctx.contains<SongFinishedTag>())) {
        if (!std::isfinite(song->scroll_speed) || song->scroll_speed <= 0.0f) {
            TraceLog(LOG_WARNING, "scroll_system skipped: invalid scroll_speed %.3f", song->scroll_speed);
            return;
        }

        auto beat_view = reg.view<ObstacleTag, WorldPosition, BeatInfo>();
        for (auto [entity, wt, info] : beat_view.each()) {
            (void)entity;
            wt.position.y = constants::SPAWN_Y
                          + (song->song_time - info.spawn_time) * song->scroll_speed;
        }
    }

}
