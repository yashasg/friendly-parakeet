#include "tutorial_dodge_hint_bind_system.h"

#include "../components/ui.h"
#include "../tags/tags.h"
#include "../util/tutorial_dodge_hint.h"
#include "web_input_policy.h"

namespace {

// Canonical DodgeHint slot position from `content/ui/screens/tutorial.rgl`
// (control `DodgeHint`). Position-keyed identification is intentional —
// the .rgl row is keyed on (x, y, w, h) and the codegen bakes those into
// the spawn block, so a layout edit that moves this slot will visibly
// break placement before the bind ever misses.
constexpr float kDodgeHintX = 110.0f;
constexpr float kDodgeHintY = 710.0f;

}  // namespace

void tutorial_dodge_hint_bind_system(entt::registry& reg) {
    const bool prefer_touch =
#if defined(PLATFORM_WEB)
        web_input_touch_capable(reg);
#elif defined(PLATFORM_HAS_KEYBOARD)
        false;
#else
        true;
#endif
    const char* text = tutorial_dodge_hint_text(prefer_touch);

    auto view = reg.view<TutorialScreenTag, UiLabelTag, UiPosition, UiLabel>();
    for (auto [e, pos, label] : view.each()) {
        (void)e;
        if (pos.x != kDodgeHintX || pos.y != kDodgeHintY) continue;
        ui_label_set(label, text);
        return;
    }
}
