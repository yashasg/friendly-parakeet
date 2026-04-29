// Title screen adapter - bridges generated rguilayout output to game systems
// Adapters translate raygui button presses into game events/actions

#include "../../components/game_state.h"
#include "../../components/ui_state.h"
#include <entt/entt.hpp>
#include <raylib.h>

// Include raygui header BEFORE the generated layout (implementation is in raygui_impl.cpp)
#include "../vendor/raygui.h"

// Include the generated header (now fully inline, no IMPLEMENTATION define needed)
#include "../generated/title_layout.h"

namespace {

TitleLayoutState layout_state;
bool initialized = false;

} // anonymous namespace

void title_adapter_init() {
    if (!initialized) {
        layout_state = TitleLayout_Init();
        initialized = true;
    }
}

void title_adapter_render(const entt::registry& /*reg*/) {
    if (!initialized) {
        title_adapter_init();
    }
    
    // Render the layout (updates button press state)
    TitleLayout_Render(&layout_state);
    
    // TODO: Translate button presses to game actions
    // When this is wired:
    // if (layout_state.ExitButtonPressed) {
    //     // Dispatch exit action (platform-gated: not on web)
    // }
    // if (layout_state.SettingsButtonPressed) {
    //     // Dispatch settings navigation
    // }
}
