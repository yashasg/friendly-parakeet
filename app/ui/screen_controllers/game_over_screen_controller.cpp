// Game Over screen controller - renders raygui layout and dispatches end-screen choices.

#include "../../components/game_state.h"
#include "../../components/scoring.h"
#include "../../components/song_state.h"
#include "screen_controller_base.h"
#include "game_over_screen_controller.h"
#include <entt/entt.hpp>

#include <raygui.h>
#include "../generated/game_over_layout.h"

#include <cstdio>

namespace {

void default_game_over_layout_render(GameOverLayoutState* state) {
    GameOverLayout_Render(state);
}

void default_draw_game_over_value(Vector2 anchor, float x, float y, float w, float h,
                                  const char* text, int text_size) {
    GameOverLayout_DrawCenteredLabel((Rectangle){anchor.x + x, anchor.y + y, w, h},
                                     text, text_size);
}

GameOverLayoutRenderHook g_layout_render_hook = &default_game_over_layout_render;
GameOverValueDrawHook g_value_draw_hook = &default_draw_game_over_value;

void game_over_layout_render(GameOverLayoutState* state) {
    g_layout_render_hook(state);
}

using GameOverController = RGuiScreenController<GameOverLayoutState,
                                                 &GameOverLayout_Init,
                                                 &game_over_layout_render>;

void draw_game_over_value(Vector2 anchor, float x, float y, float w, float h,
                          const char* text, int text_size) {
    g_value_draw_hook(anchor, x, y, w, h, text, text_size);
}

void draw_game_over_scoreboard(entt::registry& reg, const GameOverLayoutState& state) {
    const auto& score = reg.ctx().get<ScoreState>();

    char value[32] = {};
    std::snprintf(value, sizeof(value), "%d", score.score);
    draw_game_over_value(state.Anchor01, 210, 540, 300, 46, value, 36);

    std::snprintf(value, sizeof(value), "%d", score.high_score);
    draw_game_over_value(state.Anchor01, 210, 634, 300, 32, value, 24);

    if (const auto* gos = reg.ctx().find<GameOverState>()) {
        const char* reason = death_cause_text(gos->cause);
        if (reason && reason[0] != '\0') {
            draw_game_over_value(state.Anchor01, 110, 685, 500, 40, reason, 22);
        }
    }
}

} // anonymous namespace

const char* death_cause_text(DeathCause cause) {
    switch (cause) {
        case DeathCause::EnergyDepleted: return "ENERGY DEPLETED";
        case DeathCause::MissedABeat:    return "MISSED A BEAT";
        case DeathCause::None:
        default:                          return "";
    }
}

void set_game_over_screen_test_hooks(GameOverLayoutRenderHook layout_render_hook,
                                     GameOverValueDrawHook value_draw_hook) {
    g_layout_render_hook = layout_render_hook ? layout_render_hook : &default_game_over_layout_render;
    g_value_draw_hook = value_draw_hook ? value_draw_hook : &default_draw_game_over_value;
}

void reset_game_over_screen_test_hooks() {
    g_layout_render_hook = &default_game_over_layout_render;
    g_value_draw_hook = &default_draw_game_over_value;
}

void init_game_over_screen_ui() {
    // Controller state is registry-owned and initialized lazily in render.
}

void render_game_over_screen_ui(entt::registry& reg) {
    auto& controller = screen_controller<GameOverController>(reg);
    controller.render();
    draw_game_over_scoreboard(reg, controller.state());

    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase_timer <= 0.4f) return;

    dispatch_end_screen_choice(gs, controller.state());
}
