// Level Select screen adapter - renders the static rguilayout heading and Start button.
// Dynamic card/difficulty rendering is handled by draw_level_select_scene in
// ui_render_system.cpp; the raygui Start button supplements the existing hit-test path.

#include "../../components/game_state.h"
#include "adapter_base.h"
#include <entt/entt.hpp>

#include "../vendor/raygui.h"
#include "../generated/level_select_screen_layout.h"

namespace {

using LevelSelectAdapter = RGuiAdapter<LevelSelectScreenLayoutState,
                                       &LevelSelectScreenLayout_Init,
                                       &LevelSelectScreenLayout_Render>;
LevelSelectAdapter level_select_adapter;

} // anonymous namespace

void level_select_adapter_init() {
    level_select_adapter.init();
}

void level_select_adapter_render(entt::registry& reg) {
    level_select_adapter.render();

    if (level_select_adapter.state().StartButtonPressed) {
        auto& gs = reg.ctx().get<GameState>();
        if (gs.phase_timer > 0.2f) {
            auto& lss = reg.ctx().get<LevelSelectState>();
            lss.confirmed = true;
        }
    }
}
