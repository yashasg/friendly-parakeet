#include "ui_update_system.h"

#include "../components/actions.h"
#include "../components/game_state.h"
#include "../components/ui.h"
#include "../constants.h"
#include "../tags/tags.h"
#include "game_phase_transition.h"
#include "input.h"

#include <array>
#include <cstddef>

#include <raylib.h>  // CheckCollisionPointRec, Rectangle, Vector2

// Tutorial screen continue action (issue #1291).
//
// Defined in `app/ui/screen_controllers/tutorial_screen_controller.cpp`.
// Marks FTUE complete + persists settings + requests Playing phase.
// Forward-declared at namespace scope so the anonymous-namespace
// handler below can call it without a header dependency on the legacy
// controller. The controller file itself stays in tree until OoS-B
// removes all legacy controllers in one PR.
void tutorial_screen_continue(entt::registry& reg);

// ── ActionId dispatch table ─────────────────────────────────────────────────
//
// Per Fabian Principle 1 (and the explicit Keep-set rationale for
// `ActionId` in `app/.allowed-enums.txt`), the enum value is a perfect-hash
// index into a per-action function-pointer column. Each row owns the
// effect of "the user pressed this button". No central switch.
//
// Adding a new action: append the enumerator to `ActionId` (alphabetically),
// then add a row below. Codegen + the table-size assertion below catch
// mismatches at build time.

namespace {

using ActionHandler = void (*)(entt::registry&, entt::entity);

// ── Per-action handlers ─────────────────────────────────────────────
//
// `noop_action_handler` covers actions that have no consumer yet (e.g. the
// 6 screens still on the legacy controller path). The legacy controller
// renders the button and reads the press flag itself, so a no-op here
// avoids double dispatch. Migrating a screen means replacing its row's
// handler from `noop_action_handler` to the real per-action transform.

void noop_action_handler(entt::registry& /*reg*/, entt::entity /*entity*/) {
    // No consumer for this action yet — the owning screen has not migrated
    // off the legacy `screen_controllers/*` path. See #1287.
}

// Paused screen actions (migrated this cycle).
void resume_button_action(entt::registry& reg, entt::entity /*entity*/) {
    request_phase_transition<NextPhasePlayingTag>(reg);
}

void menu_button_action(entt::registry& reg, entt::entity /*entity*/) {
    request_phase_transition<NextPhaseTitleTag>(reg);
}

// Tutorial screen action (issue #1291). Calls the forward-declared
// `tutorial_screen_continue` (defined in the legacy controller TU, will
// move out in OoS-B).
void continue_button_action(entt::registry& reg, entt::entity /*entity*/) {
    tutorial_screen_continue(reg);
}

// ── Dispatch table ──────────────────────────────────────────────────
//
// Order must match the `ActionId` enumerator order in
// `app/components/actions.h`. A compile-time check at the bottom verifies
// the count.

constexpr std::array<ActionHandler, 17> kActionHandlers = {
    /* None                 */ &noop_action_handler,
    /* AudioOffsetMinus     */ &noop_action_handler,
    /* AudioOffsetPlus      */ &noop_action_handler,
    /* CloseButton          */ &noop_action_handler,
    /* ContinueButton       */ &continue_button_action,
    /* DifficultyEasy       */ &noop_action_handler,
    /* DifficultyHard       */ &noop_action_handler,
    /* DifficultyMedium     */ &noop_action_handler,
    /* ExitButton           */ &noop_action_handler,
    /* HapticsToggle        */ &noop_action_handler,
    /* LevelSelectButton    */ &noop_action_handler,
    /* MenuButton           */ &menu_button_action,
    /* PauseButton          */ &noop_action_handler,
    /* ReduceMotionToggle   */ &noop_action_handler,
    /* RestartButton        */ &noop_action_handler,
    /* ResumeButton         */ &resume_button_action,
    /* SettingsButton       */ &noop_action_handler,
};

// Bumping ActionId without bumping kActionHandlers (or vice versa) is a
// build-time error.
static_assert(kActionHandlers.size() ==
              static_cast<std::size_t>(ActionId::SettingsButton) + 1,
              "kActionHandlers size must match ActionId enumerator count");

void dispatch_action(entt::registry& reg, ActionId action, entt::entity entity) {
    const auto idx = static_cast<std::size_t>(action);
    if (idx >= kActionHandlers.size()) return;
    kActionHandlers[idx](reg, entity);
}

}  // namespace

void ui_update_system(entt::registry& reg) {
    const auto& input = reg.ctx().get<InputState>();
    if (!(input.click || input.touch_up)) return;

    // Debounce: ignore button presses inside the first UI_ENTRY_DEBOUNCE
    // seconds of the active phase so a click that opened the phase does
    // not also dismiss it (matches legacy controller behaviour, e.g.
    // `paused_screen_controller.cpp`).
    const auto& gs = reg.ctx().get<GameState>();
    if (gs.phase_timer <= constants::UI_ENTRY_DEBOUNCE) return;

    const Vector2 pointer = {input.end_x, input.end_y};

    // First-hit wins, matching raygui's GuiButton dispatch order.
    auto view = reg.view<UiPosition, UiBounds, OnPress, UiButtonTag>();
    for (auto [e, pos, sz, on_press] : view.each()) {
        const Rectangle rect{pos.x, pos.y, sz.w, sz.h};
        if (!CheckCollisionPointRec(pointer, rect)) continue;
        dispatch_action(reg, on_press.action, e);
        return;
    }
}
