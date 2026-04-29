// Song Complete screen adapter - renders raygui layout and dispatches end-screen choices.

#include "../../components/game_state.h"
#include "adapter_base.h"
#include "end_screen_dispatch.h"
#include <entt/entt.hpp>

#include "../vendor/raygui.h"
#include "../generated/song_complete_layout.h"

namespace {

using SongCompleteAdapter = RGuiAdapter<SongCompleteLayoutState,
                                        &SongCompleteLayout_Init,
                                        &SongCompleteLayout_Render>;
SongCompleteAdapter song_complete_adapter;

} // anonymous namespace

void song_complete_adapter_init() {
    song_complete_adapter.init();
}

void song_complete_adapter_render(entt::registry& reg) {
    song_complete_adapter.render();

    auto& gs = reg.ctx().get<GameState>();
    if (gs.phase_timer <= 0.5f) return;

    dispatch_end_screen_choice(gs, song_complete_adapter.state());
}
