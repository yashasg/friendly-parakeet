// Paused screen adapter - renders raygui layout and dispatches resume/menu actions.

#include "../../components/game_state.h"
#include "../../components/input.h"
#include "../../components/input_events.h"
#include <entt/entt.hpp>
#include <raylib.h>

#include "../vendor/raygui.h"
#include "../generated/paused_layout.h"

namespace {

PausedLayoutState paused_layout_state;
bool paused_initialized = false;

} // anonymous namespace

void paused_adapter_init() {
    if (!paused_initialized) {
        paused_layout_state = PausedLayout_Init();
        paused_initialized = true;
    }
}

void paused_adapter_render(entt::registry& reg) {
    if (!paused_initialized) {
        paused_adapter_init();
    }

    PausedLayout_Render(&paused_layout_state);

    auto& gs = reg.ctx().get<GameState>();

    if (paused_layout_state.ResumeButtonPressed) {
        auto mv = reg.view<MenuButtonTag>();
        reg.destroy(mv.begin(), mv.end());
        gs.previous_phase = gs.phase;
        gs.phase = GamePhase::Playing;
        gs.phase_timer = 0.0f;
    }

    if (paused_layout_state.MenuButtonPressed) {
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Title;
    }
}
