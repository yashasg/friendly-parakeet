#include "all_systems.h"
#include "../components/game_state.h"
#include "game_phase_transition.h"
#include "../constants.h"

namespace {

// Per Fabian's existential processing (issue #1202/#1204, PR G): the former
// `gs.phase == GamePhase::X` checks dispatch on tag presence in `reg.ctx()`;
// the former `gs.transition_pending = true; gs.next_phase = X` pair is now
// `request_phase_transition<NextPhase*Tag>()`. `phase_timer` is read directly
// from `GameState` because the input-delay clock is per-phase data, not a
// control-flow discriminator.
bool end_screen_input_ready(entt::registry& reg, const GameState& gs) {
    const auto& ctx = reg.ctx();
    const bool game_over_ready =
        ctx.contains<GamePhaseGameOverTag>() && gs.phase_timer > constants::GAME_OVER_INPUT_DELAY;
    const bool song_complete_ready =
        ctx.contains<GamePhaseSongCompleteTag>() && gs.phase_timer > constants::SONG_COMPLETE_INPUT_DELAY;
    return game_over_ready || song_complete_ready;
}

void erase_end_choice_tags(entt::registry& reg) {
    if (reg.ctx().find<EndChoiceRestart>())     reg.ctx().erase<EndChoiceRestart>();
    if (reg.ctx().find<EndChoiceLevelSelect>()) reg.ctx().erase<EndChoiceLevelSelect>();
    if (reg.ctx().find<EndChoiceMainMenu>())    reg.ctx().erase<EndChoiceMainMenu>();
}

// One transform per former enum value (Fabian's "transform per tag" mechanic).
// Each is gated by tag presence AND the shared input-ready predicate, so a
// choice latched during the input-delay window persists until the delay
// elapses — matching the pre-migration semantics. The handlers are mutually
// exclusive in practice because input routing only ever emplaces one tag per
// press; erase_end_choice_tags on consumption keeps that invariant explicit.
void apply_end_choice_restart(entt::registry& reg) {
    if (!reg.ctx().find<EndChoiceRestart>()) return;
    auto& gs = reg.ctx().get<GameState>();
    if (!end_screen_input_ready(reg, gs)) return;
    request_phase_transition<NextPhasePlayingTag>(reg);
    erase_end_choice_tags(reg);
}

void apply_end_choice_level_select(entt::registry& reg) {
    if (!reg.ctx().find<EndChoiceLevelSelect>()) return;
    auto& gs = reg.ctx().get<GameState>();
    if (!end_screen_input_ready(reg, gs)) return;
    request_phase_transition<NextPhaseLevelSelectTag>(reg);
    erase_end_choice_tags(reg);
}

void apply_end_choice_main_menu(entt::registry& reg) {
    if (!reg.ctx().find<EndChoiceMainMenu>()) return;
    auto& gs = reg.ctx().get<GameState>();
    if (!end_screen_input_ready(reg, gs)) return;
    request_phase_transition<NextPhaseTitleTag>(reg);
    erase_end_choice_tags(reg);
}

}  // namespace

void game_state_end_screen_system(entt::registry& reg, [[maybe_unused]] float dt) {
    apply_end_choice_restart(reg);
    apply_end_choice_level_select(reg);
    apply_end_choice_main_menu(reg);
}
