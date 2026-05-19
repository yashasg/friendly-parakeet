#include "input_routing.h"
#include "../components/game_state.h"
#include "game_phase_transition.h"
#include "input.h"
#include "input_events.h"
#include "phase_input.h"
#include "audio_events.h"
#include "haptics.h"
#include "../entities/settings.h"
#include "../constants.h"

namespace {
// Per Fabian's existential processing (issue #1202/#1204/#1277): each former
// `gs.phase == GamePhase::X` arm dispatches on the per-phase ctx-tag mirror;
// each former `MenuActionKind::Y` arm is now its own event type. The handlers
// below contain no enum-typed discriminator — the dispatcher sink for each
// event type routes straight to the matching handler.

// Returns true once a press on the active end screen has cleared the per-
// phase debounce window. Common gate for Restart / GoLevelSelect / GoMainMenu
// (which only make sense on the GameOver / SongComplete screens) and for the
// Confirm-on-end-screen passthrough below.
bool end_screen_press_unlocked(entt::registry& reg) {
    auto& gs  = reg.ctx().get<GameState>();
    auto& ctx = reg.ctx();
    const bool game_over_phase     = ctx.contains<GamePhaseGameOverTag>();
    const bool song_complete_phase = ctx.contains<GamePhaseSongCompleteTag>();
    if (!game_over_phase && !song_complete_phase) return false;

    const float input_delay = song_complete_phase
        ? constants::SONG_COMPLETE_INPUT_DELAY
        : constants::GAME_OVER_INPUT_DELAY;
    return gs.phase_timer > input_delay;
}

void enqueue_ui_haptic(entt::registry& reg, HapticEvent which) {
    if (auto* disp = reg.ctx().find<entt::dispatcher>()) {
        disp->enqueue<PlayHapticEvent>({which});
    }
}

void latch_end_choice_restart(entt::registry& reg) {
    enqueue_ui_haptic(reg, HapticEvent::RetryTap);
    reg.ctx().insert_or_assign(EndChoiceRestart{});
}
} // namespace

namespace {
// Resume from pause on any directional input (#1279). The former
// `game_state_handle_go` ignored `evt.dir` entirely — any direction
// produced the same resume request. Per-direction handlers below
// preserve that semantics with no Direction enum involvement.
void resume_from_pause_if_eligible(entt::registry& reg) {
    if (!reg.ctx().contains<GamePhasePausedTag>()) return;
    if (!phase_input_unlocked(reg)) return;
    // Deferred per #482 — let game_state_system perform the resume swap.
    request_phase_transition<NextPhasePlayingTag>(reg);
}
}  // namespace

void game_state_handle_go_up   (entt::registry& reg, const GoUpEvent&)    { resume_from_pause_if_eligible(reg); }
void game_state_handle_go_down (entt::registry& reg, const GoDownEvent&)  { resume_from_pause_if_eligible(reg); }
void game_state_handle_go_left (entt::registry& reg, const GoLeftEvent&)  { resume_from_pause_if_eligible(reg); }
void game_state_handle_go_right(entt::registry& reg, const GoRightEvent&) { resume_from_pause_if_eligible(reg); }

void game_state_handle_confirm(entt::registry& reg, const MenuConfirmEvent&) {
    auto& gs  = reg.ctx().get<GameState>();
    auto& ctx = reg.ctx();

    if (ctx.contains<GamePhaseTitleTag>()) {
        enqueue_ui_haptic(reg, HapticEvent::UIButtonTap);
        request_phase_transition<NextPhaseLevelSelectTag>(reg);
        return;
    }

    // Confirm on an end screen is the keyboard Enter shortcut for Restart.
    if (end_screen_press_unlocked(reg)) {
        latch_end_choice_restart(reg);
        return;
    }

    if (ctx.contains<GamePhaseTutorialTag>()) {
        if (!phase_input_unlocked(gs)) return;
        if (auto* settings_state = find_settings_state(reg)) {
            settings::mark_ftue_complete(*settings_state);
            settings::mark_dirty_and_save(reg, *settings_state);
        }
        request_phase_transition<NextPhasePlayingTag>(reg);
        return;
    }

    if (ctx.contains<GamePhasePausedTag>()) {
        if (!phase_input_unlocked(gs)) return;
        // Deferred per #482 — see game_state_handle_go above.
        request_phase_transition<NextPhasePlayingTag>(reg);
        return;
    }
}

void game_state_handle_restart(entt::registry& reg, const MenuRestartEvent&) {
    if (!end_screen_press_unlocked(reg)) return;
    latch_end_choice_restart(reg);
}

void game_state_handle_go_level_select(entt::registry& reg, const MenuGoLevelSelectEvent&) {
    if (!end_screen_press_unlocked(reg)) return;
    enqueue_ui_haptic(reg, HapticEvent::UIButtonTap);
    reg.ctx().insert_or_assign(EndChoiceLevelSelect{});
}

void game_state_handle_go_main_menu(entt::registry& reg, const MenuGoMainMenuEvent&) {
    if (!end_screen_press_unlocked(reg)) return;
    enqueue_ui_haptic(reg, HapticEvent::UIButtonTap);
    reg.ctx().insert_or_assign(EndChoiceMainMenu{});
}
