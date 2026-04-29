// Game Over screen adapter - renders raygui layout and dispatches end-screen choices.

#include "../../components/game_state.h"
#include "adapter_base.h"
#include "end_screen_dispatch.h"
#include <entt/entt.hpp>

#include "../vendor/raygui.h"
#include "../generated/game_over_layout.h"

namespace {

using GameOverAdapter = RGuiAdapter<GameOverLayoutState,
                                    &GameOverLayout_Init,
                                    &GameOverLayout_Render>;
GameOverAdapter game_over_adapter;

} // anonymous namespace

void game_over_adapter_init() {
    game_over_adapter.init();
}

void game_over_adapter_render(entt::registry& reg) {
    game_over_adapter.render();

    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase_timer <= 0.4f) return;

    dispatch_end_screen_choice(gs, game_over_adapter.state());
}
