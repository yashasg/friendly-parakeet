#include "all_systems.h"
#include "../components/game_state.h"
#include "game_phase_transition.h"
#include "../constants.h"

namespace {

// One transform per former enum value (Fabian's "transform per tag" mechanic,
// issue #1202/#1204, PR G). The former `gs.phase == GamePhase::X` checks now
// dispatch on per-phase tag presence in `reg.ctx()`; the former
// `gs.transition_pending = true; gs.next_phase = X` pair is now
// `request_phase_transition<NextPhase*Tag>()`. `phase_timer` is read directly
// from `GameState` because the input-delay clock is per-phase data, not a
// control-flow discriminator.
//
// Each handler is gated by tag presence AND the inline input-ready predicate,
// so a choice latched during the input-delay window persists until the delay
// elapses — matching the pre-migration semantics. The handlers are mutually
// exclusive in practice because input routing only ever emplaces one tag per
// press; erasing all three choice tags on consumption keeps that invariant
// explicit.
template <typename ChoiceTag, typename NextPhaseTag>
void apply_end_choice(entt::registry& reg) {
    if (!reg.ctx().find<ChoiceTag>()) return;
    auto& gs = reg.ctx().get<GameState>();
    const auto& ctx = reg.ctx();
    const bool game_over_ready =
        ctx.contains<GamePhaseGameOverTag>() && gs.phase_timer > constants::GAME_OVER_INPUT_DELAY;
    const bool song_complete_ready =
        ctx.contains<GamePhaseSongCompleteTag>() && gs.phase_timer > constants::SONG_COMPLETE_INPUT_DELAY;
    if (!(game_over_ready || song_complete_ready)) return;
    request_phase_transition<NextPhaseTag>(reg);
    if (reg.ctx().find<EndChoiceRestart>())     reg.ctx().erase<EndChoiceRestart>();
    if (reg.ctx().find<EndChoiceLevelSelect>()) reg.ctx().erase<EndChoiceLevelSelect>();
    if (reg.ctx().find<EndChoiceMainMenu>())    reg.ctx().erase<EndChoiceMainMenu>();
}

}  // namespace

void game_state_end_screen_system(entt::registry& reg, [[maybe_unused]] float dt) {
    apply_end_choice<EndChoiceRestart,     NextPhasePlayingTag>(reg);
    apply_end_choice<EndChoiceLevelSelect, NextPhaseLevelSelectTag>(reg);
    apply_end_choice<EndChoiceMainMenu,    NextPhaseTitleTag>(reg);
}
