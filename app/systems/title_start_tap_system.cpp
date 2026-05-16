#include "title_start_tap_system.h"

#include "../components/game_state.h"
#include "../components/ui.h"
#include "../constants.h"
#include "../tags/tags.h"
#include "game_phase_transition.h"
#include "input.h"

#include <raylib.h>  // CheckCollisionPointRec, Rectangle, Vector2

void title_start_tap_system(entt::registry& reg) {
    if (!reg.ctx().contains<GamePhaseTitleTag>()) return;

    const auto& input = reg.ctx().get<InputState>();
    if (!(input.click || input.touch_up)) return;

    const auto& gs = reg.ctx().get<GameState>();
    if (gs.phase_timer <= constants::UI_ENTRY_DEBOUNCE) return;

    const Vector2 pointer{input.end_x, input.end_y};

    // Dead-zone: any Title-screen button entity. `UiHiddenOnWebTag` is
    // INCLUDED here on purpose — Web's ExitButton bounds must continue to
    // gate the tap-anywhere transition even though the button itself does
    // not render or dispatch.
    auto buttons = reg.view<UiPosition, UiBounds, UiButtonTag, TitleScreenTag>();
    for (auto [e, pos, sz] : buttons.each()) {
        (void)e;
        const Rectangle rect{pos.x, pos.y, sz.w, sz.h};
        if (CheckCollisionPointRec(pointer, rect)) return;
    }

    request_phase_transition<NextPhaseLevelSelectTag>(reg);
}
