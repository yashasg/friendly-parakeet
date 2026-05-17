#include "ui_update_system.h"

#include "../components/actions.h"
#include "../components/game_state.h"
#include "../components/settings.h"
#include "../components/ui.h"
#include "../constants.h"
#include "../tags/tags.h"
#include "../util/level_content_config.h"
#include "game_phase_transition.h"
#include "haptics_backend.h"
#include "input.h"
#include "settings_system.h"

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

// Menu button: dispatch depends on which screen the button belongs to.
//
// End screens (Song Complete, Game Over) latch a per-choice ctx tag that
// the input-delay state machine in `game_state_end_screen_system` consumes
// after each screen's per-phase debounce — preserving the "click during
// the input-delay window persists until elapsed" semantic of the legacy
// controllers. Paused (and any future screen that goes straight to title)
// requests the phase transition directly. The branch is an existential
// tag check on the button entity, not a switch on a discriminator —
// Fabian Principle 1 still holds.
void menu_button_action(entt::registry& reg, entt::entity entity) {
    if (reg.all_of<SongCompleteScreenTag>(entity) ||
        reg.all_of<GameOverScreenTag>(entity)) {
        reg.ctx().insert_or_assign(EndChoiceMainMenu{});
        return;
    }
    request_phase_transition<NextPhaseTitleTag>(reg);
}

// End-screen actions (Song Complete migrated in #1292, Game Over migrated
// in #1293). Same per-choice ctx-tag mechanic as `menu_button_action` for
// the end-screen branch.
void restart_button_action(entt::registry& reg, entt::entity /*entity*/) {
    reg.ctx().insert_or_assign(EndChoiceRestart{});
}

// `LevelSelectButton` is emitted only from end screens (Song Complete /
// Game Over). On the Level Select screen itself, card presses skip the
// `ActionId` dispatch entirely — see Pass C in `ui_update_system` below,
// which writes the card's `LevelIndex` into `LevelSelectState` directly
// (cards carry no `OnPress`, by design — issue #1296).
void level_select_button_action(entt::registry& reg, entt::entity /*entity*/) {
    reg.ctx().insert_or_assign(EndChoiceLevelSelect{});
}

// Tutorial screen action (issue #1291). Calls the forward-declared
// `tutorial_screen_continue` (defined in the legacy controller TU, will
// move out in OoS-B).
void continue_button_action(entt::registry& reg, entt::entity /*entity*/) {
    tutorial_screen_continue(reg);
}

// Title screen actions (issue #1294).
//
// `exit_button_action`: native-only quit. On PLATFORM_WEB the ExitButton
// entity carries `UiHiddenOnWebTag` and is skipped by `ui_update_system`
// before dispatch, so the platform guard here is defense-in-depth — the
// handler also covers callers (e.g. tests) that simulate a press by
// invoking the dispatcher directly.
void exit_button_action(entt::registry& reg, entt::entity /*entity*/) {
#ifndef PLATFORM_WEB
    reg.ctx().get<InputState>().quit_requested = true;
#else
    (void)reg;
#endif
}

void settings_button_action(entt::registry& reg, entt::entity /*entity*/) {
    request_phase_transition<NextPhaseSettingsTag>(reg);
}

// Settings screen actions (issue #1295).
//
// AudioOffsetMinus / AudioOffsetPlus step `SettingsState::audio_offset_ms`
// by `AUDIO_OFFSET_STEP_MS` and clamp to [MIN, MAX]. CloseButton routes
// back to Title (no other screen currently emits ActionId::CloseButton).
// HapticsToggle / ReduceMotionToggle flip their respective bool and
// persist via `settings::mark_dirty_and_save`. Enabling haptics also
// warms up the platform backend (latency-sensitive first vibration,
// matches legacy controller).

void audio_offset_step_action(entt::registry& reg, int delta) {
    auto* st = find_settings_state(reg);
    if (st == nullptr) return;
    const auto before = st->audio_offset_ms;
    const int candidate = static_cast<int>(st->audio_offset_ms) + delta;
    const int clamped = (candidate < SettingsState::MIN_AUDIO_OFFSET_MS)
                      ? SettingsState::MIN_AUDIO_OFFSET_MS
                      : ((candidate > SettingsState::MAX_AUDIO_OFFSET_MS)
                         ? SettingsState::MAX_AUDIO_OFFSET_MS
                         : candidate);
    st->audio_offset_ms = static_cast<int16_t>(clamped);
    if (st->audio_offset_ms != before) {
        settings::mark_dirty_and_save(reg, *st);
    }
}

void audio_offset_minus_action(entt::registry& reg, entt::entity /*entity*/) {
    audio_offset_step_action(reg, -SettingsState::AUDIO_OFFSET_STEP_MS);
}

void audio_offset_plus_action(entt::registry& reg, entt::entity /*entity*/) {
    audio_offset_step_action(reg, +SettingsState::AUDIO_OFFSET_STEP_MS);
}

void close_button_action(entt::registry& reg, entt::entity /*entity*/) {
    request_phase_transition<NextPhaseTitleTag>(reg);
}

void haptics_toggle_action(entt::registry& reg, entt::entity /*entity*/) {
    auto* st = find_settings_state(reg);
    if (st == nullptr) return;
    st->haptics_enabled = !st->haptics_enabled;
    settings::mark_dirty_and_save(reg, *st);
    if (st->haptics_enabled) {
        platform::haptics::warmup(reg);
    }
}

void reduce_motion_toggle_action(entt::registry& reg, entt::entity /*entity*/) {
    auto* st = find_settings_state(reg);
    if (st == nullptr) return;
    st->reduce_motion = !st->reduce_motion;
    settings::mark_dirty_and_save(reg, *st);
}

// Level Select screen actions (issue #1296).
//
// `StartButton` latches `LevelSelectConfirmedTag` on the ctx; the
// `game_state_system` LevelSelect block consumes it after the entry
// debounce and routes to Tutorial (if FTUE incomplete) or Playing.
//
// `DifficultyEasy/Medium/Hard` write the button's `DifficultyIndex` into
// `LevelSelectState::selected_difficulty`. Per Fabian Principle 1 the
// effect lives in one handler reading from the entity's
// `DifficultyIndex` rather than three near-identical handlers — the
// dispatch table still indexes by `ActionId` (#1287 invariant) and each
// row points at this shared transform.
//
// Both write-paths are gated by the entity's `LevelIndex` matching the
// currently selected level. Diff buttons for non-active cards exist but
// are invisible per the render filter; this guard is defense-in-depth
// against the (impossible by render contract, but cheap to verify)
// "click on hidden button" case.
void start_button_action(entt::registry& reg, entt::entity /*entity*/) {
    reg.ctx().insert_or_assign(LevelSelectConfirmedTag{});
}

void apply_difficulty_action(entt::registry& reg, entt::entity entity) {
    const auto* level_idx = reg.try_get<LevelIndex>(entity);
    const auto* diff_idx  = reg.try_get<DifficultyIndex>(entity);
    if (level_idx == nullptr || diff_idx == nullptr) return;
    auto& lss = reg.ctx().get<LevelSelectState>();
    if (level_idx->value != lss.selected_level) return;
    if (!content_config::is_valid_difficulty_index(diff_idx->value)) return;
    lss.selected_difficulty = diff_idx->value;
}

// ── Dispatch table ──────────────────────────────────────────────────
//
// Order must match the `ActionId` enumerator order in
// `app/components/actions.h`. A compile-time check at the bottom verifies
// the count.

constexpr std::array<ActionHandler, 18> kActionHandlers = {
    /* None                 */ &noop_action_handler,
    /* AudioOffsetMinus     */ &audio_offset_minus_action,
    /* AudioOffsetPlus      */ &audio_offset_plus_action,
    /* CloseButton          */ &close_button_action,
    /* ContinueButton       */ &continue_button_action,
    /* DifficultyEasy       */ &apply_difficulty_action,
    /* DifficultyHard       */ &apply_difficulty_action,
    /* DifficultyMedium     */ &apply_difficulty_action,
    /* ExitButton           */ &exit_button_action,
    /* HapticsToggle        */ &haptics_toggle_action,
    /* LevelSelectButton    */ &level_select_button_action,
    /* MenuButton           */ &menu_button_action,
    /* PauseButton          */ &noop_action_handler,
    /* ReduceMotionToggle   */ &reduce_motion_toggle_action,
    /* RestartButton        */ &restart_button_action,
    /* ResumeButton         */ &resume_button_action,
    /* SettingsButton       */ &settings_button_action,
    /* StartButton          */ &start_button_action,
};

// Bumping ActionId without bumping kActionHandlers (or vice versa) is a
// build-time error.
static_assert(kActionHandlers.size() ==
              static_cast<std::size_t>(ActionId::StartButton) + 1,
              "kActionHandlers size must match ActionId enumerator count");

void dispatch_action(entt::registry& reg, ActionId action, entt::entity entity) {
    const auto idx = static_cast<std::size_t>(action);
    if (idx >= kActionHandlers.size()) return;
    kActionHandlers[idx](reg, entity);
}

// ── Per-active-phase input-delay table ──────────────────────────────────
//
// Each migrated end-screen exposes its own debounce constant (Game Over
// uses 0.4s, Song Complete uses 0.5s, plain UI screens use the shared
// 0.2s `UI_ENTRY_DEBOUNCE`). Per Fabian Principle 1, the lookup is a
// table indexed by active-phase tag, not a switch on a phase enum.
// Adding a screen that needs a custom debounce: append one row.

template <typename PhaseTag>
bool phase_tag_present(const entt::registry& reg) noexcept {
    return reg.ctx().contains<PhaseTag>();
}

struct PhaseDebounceRow {
    bool (*phase_active)(const entt::registry&);
    float seconds;
};

constexpr std::array<PhaseDebounceRow, 2> kPhaseDebounceRows = {{
    {&phase_tag_present<GamePhaseGameOverTag>,     constants::GAME_OVER_INPUT_DELAY},
    {&phase_tag_present<GamePhaseSongCompleteTag>, constants::SONG_COMPLETE_INPUT_DELAY},
}};

float effective_debounce(const entt::registry& reg) {
    for (const auto& row : kPhaseDebounceRows) {
        if (row.phase_active(reg)) return row.seconds;
    }
    return constants::UI_ENTRY_DEBOUNCE;
}

}  // namespace

void ui_update_system(entt::registry& reg) {
    const auto& input = reg.ctx().get<InputState>();
    if (!(input.click || input.touch_up)) return;

    // Debounce: ignore button presses inside the active phase's input-delay
    // window so a click that opened the phase does not also dismiss it,
    // and so end-screen prompts honour their longer settling time before
    // accepting input (matches legacy controller behaviour, e.g.
    // `paused_screen_controller.cpp` and `song_complete_screen_controller.cpp`).
    const auto& gs = reg.ctx().get<GameState>();
    if (gs.phase_timer <= effective_debounce(reg)) return;

    const Vector2 pointer = {input.end_x, input.end_y};

    // Hit-test runs as three priority passes; smaller-hit-area-wins matches
    // the legacy raygui dispatch order (an overlay button drawn on top of a
    // card consumes the click that the underlying card would otherwise
    // catch). Each pass returns on the first hit; later passes only see
    // unconsumed clicks.
    //
    // Pass A — Level Select difficulty buttons for the currently active
    // card only (issue #1296). Inactive-card diff buttons are invisible
    // per the render filter and must not block the underlying card from
    // claiming the click; the active-level guard keeps them transparent
    // to hit-test until their card becomes active.
    {
        const auto* lss = reg.ctx().find<LevelSelectState>();
        if (lss != nullptr) {
            auto diff_view = reg.view<UiPosition, UiBounds, OnPress,
                                      UiButtonTag, DifficultyButtonTag,
                                      LevelIndex>();
            for (auto entity : diff_view) {
                const auto& lidx = diff_view.template get<LevelIndex>(entity);
                if (lidx.value != lss->selected_level) continue;
                const auto& pos = diff_view.template get<UiPosition>(entity);
                const auto& sz  = diff_view.template get<UiBounds>(entity);
                const Rectangle rect{pos.x, pos.y, sz.w, sz.h};
                if (!CheckCollisionPointRec(pointer, rect)) continue;
                const auto& on_press = diff_view.template get<OnPress>(entity);
                dispatch_action(reg, on_press.action, entity);
                return;
            }
        }
    }

    // Pass B — regular buttons. First-hit wins, matching raygui's GuiButton
    // dispatch order. Entities carrying `UiHiddenOnWebTag` (e.g.
    // Title-screen ExitButton on Web per #511) are excluded from dispatch
    // on PLATFORM_WEB — they still occupy bounds for tap-anywhere
    // dead-zone purposes elsewhere (see `title_start_tap_system`), but
    // pressing them must not fire an action. `DifficultyButtonTag` is
    // excluded so Pass A is the sole handler for that archetype (its
    // active-level filter is the entire selection rule).
    {
        auto view = reg.view<UiPosition, UiBounds, OnPress, UiButtonTag>(
            entt::exclude<DifficultyButtonTag
#ifdef PLATFORM_WEB
                          , UiHiddenOnWebTag
#endif
                          >
        );
        for (auto [e, pos, sz, on_press] : view.each()) {
            const Rectangle rect{pos.x, pos.y, sz.w, sz.h};
            if (!CheckCollisionPointRec(pointer, rect)) continue;
            dispatch_action(reg, on_press.action, e);
            return;
        }
    }

    // Pass C — Level Select cards (issue #1296). Cards are not buttons in
    // the existential sense (they carry no `OnPress`); a card click sets
    // `LevelSelectState::selected_level` directly. Running last means
    // diff buttons (Pass A) and other buttons (Pass B) get priority,
    // since the card rectangle visually contains the diff button row.
    {
        if (auto* lss = reg.ctx().find<LevelSelectState>()) {
            auto card_view = reg.view<UiPosition, UiBounds, LevelCardTag,
                                      LevelIndex>();
            for (auto [e, pos, sz, lidx] : card_view.each()) {
                (void)e;
                const Rectangle rect{pos.x, pos.y, sz.w, sz.h};
                if (!CheckCollisionPointRec(pointer, rect)) continue;
                if (!content_config::is_valid_level_index(lidx.value)) continue;
                lss->selected_level = lidx.value;
                return;
            }
        }
    }
}
