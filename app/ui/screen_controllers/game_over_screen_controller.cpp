// Game Over screen controller - renders raygui layout and dispatches end-screen choices.

#include "../../components/game_state.h"
#include "screen_controller_base.h"
#include <entt/entt.hpp>

#include <raygui.h>
#include "../generated/game_over_layout.h"

namespace {

using GameOverController = RGuiScreenController<GameOverLayoutState,
                                                 &GameOverLayout_Init,
                                                 &GameOverLayout_Render>;

} // anonymous namespace

void init_game_over_screen_ui() {
    // Controller state is registry-owned and initialized lazily in render.
}

void render_game_over_screen_ui(entt::registry& reg) {
    auto& controller = screen_controller<GameOverController>(reg);
    controller.render();

    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase_timer <= 0.4f) return;

    dispatch_end_screen_choice(gs, controller.state());
}
