// Paused screen controller - renders raygui layout and dispatches resume/menu actions.

#include "../../components/game_state.h"
#include "../../components/input.h"
#include "screen_controller_base.h"
#include <entt/entt.hpp>

#include <raygui.h>
#include "../generated/paused_layout.h"

namespace {

using PausedController = RGuiScreenController<PausedLayoutState,
                                               &PausedLayout_Init,
                                               &PausedLayout_Render>;

} // anonymous namespace

void init_paused_screen_ui() {
    // Controller state is registry-owned and initialized lazily in render.
}

void render_paused_screen_ui(entt::registry& reg) {
    auto& controller = screen_controller<PausedController>(reg);
    controller.render();

    auto& gs = reg.ctx().get<GameState>();

    if (controller.state().ResumeButtonPressed) {
        // Deferred per #482 — the canonical state-machine swap (game_state_system)
        // routes Paused→Playing as a resume that preserves session state.
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Playing;
    }

    if (controller.state().MenuButtonPressed) {
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Title;
    }
}
