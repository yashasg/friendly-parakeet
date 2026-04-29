// Level Select screen adapter - renders the static rguilayout heading and Start button.
// Dynamic card/difficulty rendering is handled by draw_level_select_scene in
// ui_render_system.cpp; the raygui Start button supplements the existing hit-test path.

#include "../../components/game_state.h"
#include <entt/entt.hpp>
#include <raylib.h>

#include "../vendor/raygui.h"
#include "../generated/level_select_screen_layout.h"

namespace {

LevelSelectScreenLayoutState level_select_screen_layout_state;
bool level_select_screen_initialized = false;

} // anonymous namespace

void level_select_adapter_init() {
    if (!level_select_screen_initialized) {
        level_select_screen_layout_state = LevelSelectScreenLayout_Init();
        level_select_screen_initialized = true;
    }
}

void level_select_adapter_render(entt::registry& reg) {
    if (!level_select_screen_initialized) {
        level_select_adapter_init();
    }

    LevelSelectScreenLayout_Render(&level_select_screen_layout_state);

    if (level_select_screen_layout_state.StartButtonPressed) {
        auto& gs = reg.ctx().get<GameState>();
        if (gs.phase_timer > 0.2f) {
            auto& lss = reg.ctx().get<LevelSelectState>();
            lss.confirmed = true;
        }
    }
}
