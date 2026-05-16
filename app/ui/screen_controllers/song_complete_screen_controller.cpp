// Song Complete screen controller - renders raygui layout and dispatches end-screen choices.

#include "../../components/game_state.h"
#include "../../components/scoring.h"
#include "../../components/song_state.h"
#include "../../components/energy_bar.h"
#include "../../constants.h"
#include "screen_controller_base.h"
#include <entt/entt.hpp>

#include <raygui.h>
#include "../generated/song_complete_layout.h"

#include <cstdio>

namespace {

using SongCompleteController = RGuiScreenController<SongCompleteLayoutState,
                                                     &SongCompleteLayout_Init,
                                                     &SongCompleteLayout_Render>;

void draw_song_complete_value(Vector2 anchor, float x, float y, float w, float h,
                               const char* text, int text_size) {
    SongCompleteLayout_DrawCenteredLabel(Rectangle{anchor.x + x, anchor.y + y, w, h},
                                          text, text_size);
}

void draw_song_complete_scoreboard(entt::registry& reg, const SongCompleteLayoutState& state) {
    const auto& score = reg.ctx().get<ScoreState>();
    const auto& current = reg.ctx().get<CurrentSongHighScore>();

    char value[32] = {};
    std::snprintf(value, sizeof(value), "%d", score.score);
    draw_song_complete_value(state.Anchor01, 160, 463, 400, 50, value, 36);

    std::snprintf(value, sizeof(value), "%d", current.value);
    draw_song_complete_value(state.Anchor01, 160, 573, 400, 50, value, 36);

    if (const auto* result = reg.ctx().find<TerminalResultState>(); result && result->new_best) {
        char best_line[48] = {};
        std::snprintf(best_line, sizeof(best_line), "NEW BEST! PREV %d", result->previous_best);
        draw_song_complete_value(state.Anchor01, 120, 620, 480, 28, best_line, 22);
    }

    const auto* results = reg.ctx().find<SongResults>();
    if (!results) return;

    char line[96] = {};
    std::snprintf(line, sizeof(line), "PERFECT %d     GOOD %d",
                  results->perfect_count, results->good_count);
    draw_song_complete_value(state.Anchor01, 120, 650, 480, 30, line, 22);

    std::snprintf(line, sizeof(line), "OK %d     BAD %d     MISS %d",
                  results->ok_count, results->bad_count, results->miss_count);
    draw_song_complete_value(state.Anchor01, 120, 684, 480, 30, line, 22);

    std::snprintf(line, sizeof(line), "MAX CHAIN %d", results->max_chain);
    draw_song_complete_value(state.Anchor01, 120, 718, 480, 30, line, 22);

    const auto* energy = reg.ctx().find<EnergyState>();
    if (energy) {
        std::snprintf(line, sizeof(line), "ENERGY %.0f%%", static_cast<double>(energy->energy * 100.0f));
        draw_song_complete_value(state.Anchor01, 120, 752, 480, 30, line, 22);
    }
}

} // anonymous namespace

void init_song_complete_screen_ui() {
    // Controller state is registry-owned and initialized lazily in render.
}

void render_song_complete_screen_ui(entt::registry& reg) {
    auto& controller = screen_controller<SongCompleteController>(reg);
    controller.render();
    draw_song_complete_scoreboard(reg, controller.state());

    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase_timer <= constants::SONG_COMPLETE_INPUT_DELAY) return;

    dispatch_end_screen_choice(reg, controller.state());
}
