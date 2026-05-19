#include "ui_update_system.h"

#include "../components/game_state.h"
#include "../components/settings.h"
#include "../components/ui.h"
#include "../constants.h"
#include "../tags/tags.h"
#include "../util/level_content_config.h"
#include "game_phase_transition.h"
#include "haptics_backend.h"
#include "input.h"
#include "input_events.h"
#include "phase_input.h"
#include "settings_system.h"

#include <array>
#include <cstddef>

#include <raylib.h>  // CheckCollisionPointRec, Rectangle, Vector2

// Tutorial screen continue action (issue #1291).
// Marks FTUE complete + persists settings + requests Playing phase.
// Exposed via `ui_update_system.h` so the FTUE end-to-end test in
// `tests/test_game_state_extended.cpp` can drive it directly; the
// `UiActionContinueButtonTag` button path below dispatches to it.
void tutorial_screen_continue(entt::registry& reg) {
    if (auto* settings_ptr = find_settings_state(reg)) {
        settings::mark_ftue_complete(*settings_ptr);
        settings::mark_dirty_and_save(reg, *settings_ptr);
    }
    request_phase_transition<NextPhasePlayingTag>(reg);
}

// ── UI action membership dispatch ───────────────────────────────────────────
//
// Button behavior is selected by ECS membership: generated buttons carry a
// `UiAction*Tag`, and the hit-test path invokes the system whose tag is present.
// `ActionId` remains only as a codegen validation label for `.rgl` names.

namespace {

// Paused screen actions.
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
// Game Over). On the Level Select screen itself, card presses skip button
// action dispatch entirely — see Pass C in `ui_update_system` below, which
// writes the card's `LevelIndex` into `LevelSelectState` directly (cards
// carry no `UiButtonTag`, by design — issue #1296).
void level_select_button_action(entt::registry& reg, entt::entity /*entity*/) {
    reg.ctx().insert_or_assign(EndChoiceLevelSelect{});
}

// Tutorial screen action (issue #1291). Dispatches to
// `tutorial_screen_continue` (defined above).
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
// back to Title (no other screen currently emits `UiActionCloseButtonTag`).
// HapticsToggle / ReduceMotionToggle flip their respective bool and
// persist via `settings::mark_dirty_and_save`. Enabling haptics also
// warms up the platform backend so the first vibration after enable is
// not latency-bound.

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
// `UiActionDifficultyEasy/Medium/HardTag` buttons write the button's
// `DifficultyIndex` into `LevelSelectState::selected_difficulty`. Per
// Fabian Principle 1 the selected difficulty is carried by the entity row,
// not a behavior discriminator.
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

// Gameplay HUD pause action (issue #1297). The pre-migration Pause path
// called `request_phase_transition<NextPhasePausedTag>` directly and
// silently skipped Pause outside `GamePhasePlayingTag`; the same guard
// lives here.
void pause_button_action(entt::registry& reg, entt::entity /*entity*/) {
    if (!reg.ctx().contains<GamePhasePlayingTag>()) return;
    request_phase_transition<NextPhasePausedTag>(reg);
}

template <typename ActionTag>
bool try_run_action(entt::registry& reg,
                    entt::entity entity,
                    void (*handler)(entt::registry&, entt::entity)) {
    if (!reg.all_of<ActionTag>(entity)) return false;
    handler(reg, entity);
    return true;
}

void dispatch_pressed_button(entt::registry& reg, entt::entity entity) {
    if (try_run_action<UiActionAudioOffsetMinusTag>(reg, entity, &audio_offset_minus_action)) return;
    if (try_run_action<UiActionAudioOffsetPlusTag>(reg, entity, &audio_offset_plus_action)) return;
    if (try_run_action<UiActionCloseButtonTag>(reg, entity, &close_button_action)) return;
    if (try_run_action<UiActionContinueButtonTag>(reg, entity, &continue_button_action)) return;
    if (try_run_action<UiActionDifficultyEasyTag>(reg, entity, &apply_difficulty_action)) return;
    if (try_run_action<UiActionDifficultyHardTag>(reg, entity, &apply_difficulty_action)) return;
    if (try_run_action<UiActionDifficultyMediumTag>(reg, entity, &apply_difficulty_action)) return;
    if (try_run_action<UiActionExitButtonTag>(reg, entity, &exit_button_action)) return;
    if (try_run_action<UiActionHapticsToggleTag>(reg, entity, &haptics_toggle_action)) return;
    if (try_run_action<UiActionLevelSelectButtonTag>(reg, entity, &level_select_button_action)) return;
    if (try_run_action<UiActionMenuButtonTag>(reg, entity, &menu_button_action)) return;
    if (try_run_action<UiActionPauseButtonTag>(reg, entity, &pause_button_action)) return;
    if (try_run_action<UiActionReduceMotionToggleTag>(reg, entity, &reduce_motion_toggle_action)) return;
    if (try_run_action<UiActionRestartButtonTag>(reg, entity, &restart_button_action)) return;
    if (try_run_action<UiActionResumeButtonTag>(reg, entity, &resume_button_action)) return;
    if (try_run_action<UiActionSettingsButtonTag>(reg, entity, &settings_button_action)) return;
    if (try_run_action<UiActionStartButtonTag>(reg, entity, &start_button_action)) return;
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

// ── Lane button hit-test (issue #1297) ──────────────────────────────
//
// Per Fabian Principle 1, each (shape-tag, event) pairing is its own
// row in the dispatch table; the loop iterates the table instead of
// switching on a Shape enum. Adding a new shape: append one row.
struct LaneButtonRow {
    bool (*try_dispatch)(entt::registry&, Vector2);
};

template <typename ShapeTag, typename Event>
bool try_lane_button_for(entt::registry& reg, Vector2 pointer) {
    auto view = reg.view<LaneButtonTag, UiPosition, UiBounds, ShapeTag>();
    for (auto entity : view) {
        const auto& pos = view.template get<UiPosition>(entity);
        const auto& sz  = view.template get<UiBounds>(entity);
        const Rectangle rect{pos.x, pos.y, sz.w, sz.h};
        if (!CheckCollisionPointRec(pointer, rect)) continue;
        auto& disp = reg.ctx().get<entt::dispatcher>();
        disp.enqueue<Event>(Event{});
        return true;
    }
    return false;
}

constexpr std::array<LaneButtonRow, 3> kLaneButtonRows = {{
    {&try_lane_button_for<UiShapeIconCircleTag,   ShapePressCircleEvent>},
    {&try_lane_button_for<UiShapeIconSquareTag,   ShapePressSquareEvent>},
    {&try_lane_button_for<UiShapeIconTriangleTag, ShapePressTriangleEvent>},
}};

bool try_dispatch_lane_button(entt::registry& reg, Vector2 pointer) {
    for (const auto& row : kLaneButtonRows) {
        if (row.try_dispatch(reg, pointer)) return true;
    }
    return false;
}

}  // namespace

void ui_update_system(entt::registry& reg) {
    const auto& input = reg.ctx().get<InputState>();
    if (!(input.click || input.touch_up || input.button_touch_up)) return;

    // Debounce: ignore button presses inside the active phase's input-delay
    // window so a click that opened the phase does not also dismiss it,
    // and so end-screen prompts honour their longer settling time before
    // accepting input. The phase-row table picks the per-phase delay; any
    // phase not listed falls through to the default `UI_ENTRY_DEBOUNCE`.
    const auto& gs = reg.ctx().get<GameState>();
    float debounce_sec = constants::UI_ENTRY_DEBOUNCE;
    for (const auto& row : kPhaseDebounceRows) {
        if (row.phase_active(reg)) { debounce_sec = row.seconds; break; }
    }
    if (!phase_input_unlocked(gs, debounce_sec)) return;

    // ── Pass D — Gameplay HUD lane buttons (issue #1297) ─────────────
    //
    // The three shape buttons (Circle / Square / Triangle) live in the
    // touch button zone (y >= SCREEN_H * SWIPE_ZONE_SPLIT). A touch that
    // started inside that zone produces `button_touch_up` on release
    // with the release coordinates in `button_end_x/y`; a desktop mouse
    // click produces `click` with the release coordinates in `end_x/y`.
    // A swipe-zone touch release (`touch_up && !button_touch_up`) must
    // NOT trigger a shape press even if it ends over the lane button
    // bounds — that case is a directional swipe whose endpoint happens
    // to land on the HUD (legacy guard in #986).
    //
    // Per Fabian Principle 1 (existential processing): each shape's
    // sub-view + matching `ShapePress*Event` is its own row in the
    // implicit table; there is no `switch (Shape)` anywhere here.
    //
    // Button-zone touches that miss every shape button are consumed
    // anyway (the `return` below): legacy semantics in
    // `gameplay_hud_process_button_input` checked only shape buttons on
    // `button_touch_up`, so a missed shape press must not fall through
    // and accidentally trigger Pause or any other regular button on its
    // way out.
    const bool playing_phase = reg.ctx().contains<GamePhasePlayingTag>();
    if (input.button_touch_up) {
        if (playing_phase) {
            try_dispatch_lane_button(reg,
                Vector2{input.button_end_x, input.button_end_y});
        }
        return;
    }
    if (input.click && playing_phase) {
        if (try_dispatch_lane_button(reg, Vector2{input.end_x, input.end_y})) {
            return;
        }
    }

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
            auto diff_view = reg.view<UiPosition, UiBounds,
                                      UiButtonTag, DifficultyButtonTag,
                                      LevelIndex>();
            for (auto entity : diff_view) {
                const auto& lidx = diff_view.template get<LevelIndex>(entity);
                if (lidx.value != lss->selected_level) continue;
                const auto& pos = diff_view.template get<UiPosition>(entity);
                const auto& sz  = diff_view.template get<UiBounds>(entity);
                const Rectangle rect{pos.x, pos.y, sz.w, sz.h};
                if (!CheckCollisionPointRec(pointer, rect)) continue;
                dispatch_pressed_button(reg, entity);
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
        auto view = reg.view<UiPosition, UiBounds, UiButtonTag>(
            entt::exclude<DifficultyButtonTag
#ifdef PLATFORM_WEB
                          , UiHiddenOnWebTag
#endif
                          >
        );
        for (auto [e, pos, sz] : view.each()) {
            const Rectangle rect{pos.x, pos.y, sz.w, sz.h};
            if (!CheckCollisionPointRec(pointer, rect)) continue;
            dispatch_pressed_button(reg, e);
            return;
        }
    }

    // Pass C — Level Select cards (issue #1296). Cards are not buttons in
    // the existential sense (they carry no `UiButtonTag`); a card click sets
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
