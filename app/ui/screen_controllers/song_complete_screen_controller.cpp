// Song Complete screen controller - renders raygui layout and dispatches end-screen choices.

#include "../../components/game_state.h"
#include "../../components/scoring.h"
#include "../../components/song_state.h"
#include "screen_controller_base.h"
#include <entt/entt.hpp>

#include <raygui.h>
#include "../generated/song_complete_layout.h"

#include <cstdio>

namespace {

using SongCompleteController = RGuiScreenController<SongCompleteLayoutState,
                                                     &SongCompleteLayout_Init,
                                                     &SongCompleteLayout_Render>;
SongCompleteController song_complete_controller;

void draw_song_complete_value(Vector2 anchor, float x, float y, float w, float h,
                              const char* text, int text_size) {
    SongCompleteLayout_DrawCenteredLabel((Rectangle){anchor.x + x, anchor.y + y, w, h},
                                         text, text_size);
}

void draw_song_complete_scoreboard(entt::registry& reg, const SongCompleteLayoutState& state) {
    const auto& score = reg.ctx().get<ScoreState>();

    char value[32] = {};
    std::snprintf(value, sizeof(value), "%d", score.score);
    draw_song_complete_value(state.Anchor01, 160, 463, 400, 50, value, 36);

    std::snprintf(value, sizeof(value), "%d", score.high_score);
    draw_song_complete_value(state.Anchor01, 160, 573, 400, 50, value, 36);

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
}

} // anonymous namespace

void init_song_complete_screen_ui() {
    song_complete_controller.init();
}

void render_song_complete_screen_ui(entt::registry& reg) {
    song_complete_controller.render();
    draw_song_complete_scoreboard(reg, song_complete_controller.state());

    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase_timer <= 0.5f) return;

    dispatch_end_screen_choice(gs, song_complete_controller.state());
}
