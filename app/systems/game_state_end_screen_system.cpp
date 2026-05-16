#include "all_systems.h"
#include "../components/game_state.h"
#include "../constants.h"

namespace {

bool end_screen_input_ready(const GameState& gs) {
    const bool game_over_ready =
        gs.phase == GamePhase::GameOver && gs.phase_timer > constants::GAME_OVER_INPUT_DELAY;
    const bool song_complete_ready =
        gs.phase == GamePhase::SongComplete && gs.phase_timer > constants::SONG_COMPLETE_INPUT_DELAY;
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
    if (!end_screen_input_ready(gs)) return;
    gs.transition_pending = true;
    gs.next_phase = GamePhase::Playing;
    erase_end_choice_tags(reg);
}

void apply_end_choice_level_select(entt::registry& reg) {
    if (!reg.ctx().find<EndChoiceLevelSelect>()) return;
    auto& gs = reg.ctx().get<GameState>();
    if (!end_screen_input_ready(gs)) return;
    gs.transition_pending = true;
    gs.next_phase = GamePhase::LevelSelect;
    erase_end_choice_tags(reg);
}

void apply_end_choice_main_menu(entt::registry& reg) {
    if (!reg.ctx().find<EndChoiceMainMenu>()) return;
    auto& gs = reg.ctx().get<GameState>();
    if (!end_screen_input_ready(gs)) return;
    gs.transition_pending = true;
    gs.next_phase = GamePhase::Title;
    erase_end_choice_tags(reg);
}

}  // namespace

void game_state_end_screen_system(entt::registry& reg, [[maybe_unused]] float dt) {
    apply_end_choice_restart(reg);
    apply_end_choice_level_select(reg);
    apply_end_choice_main_menu(reg);
}
