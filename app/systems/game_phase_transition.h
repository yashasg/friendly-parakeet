#pragma once

#include <entt/entt.hpp>

#include "../components/game_state.h"

// ── Phase-transition API (issue #1202 PR G) ───────────────────────────────
//
// Per Fabian's existential processing, the former `GamePhase` enum is gone.
// Each former enum value is now a zero-column ctx table:
//   - `GamePhase*Tag`  : the active phase (exactly one present at any time).
//   - `NextPhase*Tag`  : the pending-transition target (exactly zero outside
//                       the dispatch block, exactly one between
//                       `request_phase_transition<...>` and
//                       `clear_next_phase_tags`).
//
// Public entry points dispatch on the tag type at the *call site* — there is
// no runtime enum compare, no `switch`, no virtual dispatch. The compiler
// instantiates one transform per tag the program actually uses, exactly
// matching the "transform per row of the former enum table" mechanic.
//
// Direct callers of `enter_phase<...>` are restricted by the architectural
// allow-list pinned by `tests/test_phase_transition_canonical.cpp`
// (issue #482/#503): only `play_session.cpp`, `game_state_system.cpp`, and
// `game_state_terminal_phase_system.cpp` may invoke it. UI and input
// systems request transitions via `request_phase_transition<...>` and let
// `game_state_system` perform the swap on the next tick (issue #482).

namespace phase_transition_detail {

inline void erase_all_game_phase_tags(entt::registry& reg) {
    auto& ctx = reg.ctx();
    if (ctx.find<GamePhaseTitleTag>())        ctx.erase<GamePhaseTitleTag>();
    if (ctx.find<GamePhaseLevelSelectTag>())  ctx.erase<GamePhaseLevelSelectTag>();
    if (ctx.find<GamePhasePlayingTag>())      ctx.erase<GamePhasePlayingTag>();
    if (ctx.find<GamePhasePausedTag>())       ctx.erase<GamePhasePausedTag>();
    if (ctx.find<GamePhaseGameOverTag>())     ctx.erase<GamePhaseGameOverTag>();
    if (ctx.find<GamePhaseSongCompleteTag>()) ctx.erase<GamePhaseSongCompleteTag>();
    if (ctx.find<GamePhaseSettingsTag>())     ctx.erase<GamePhaseSettingsTag>();
    if (ctx.find<GamePhaseTutorialTag>())     ctx.erase<GamePhaseTutorialTag>();
}

inline void erase_all_next_phase_tags(entt::registry& reg) {
    auto& ctx = reg.ctx();
    if (ctx.find<NextPhaseTitleTag>())        ctx.erase<NextPhaseTitleTag>();
    if (ctx.find<NextPhaseLevelSelectTag>())  ctx.erase<NextPhaseLevelSelectTag>();
    if (ctx.find<NextPhasePlayingTag>())      ctx.erase<NextPhasePlayingTag>();
    if (ctx.find<NextPhasePausedTag>())       ctx.erase<NextPhasePausedTag>();
    if (ctx.find<NextPhaseGameOverTag>())     ctx.erase<NextPhaseGameOverTag>();
    if (ctx.find<NextPhaseSongCompleteTag>()) ctx.erase<NextPhaseSongCompleteTag>();
    if (ctx.find<NextPhaseSettingsTag>())     ctx.erase<NextPhaseSettingsTag>();
    if (ctx.find<NextPhaseTutorialTag>())     ctx.erase<NextPhaseTutorialTag>();
}

// WASM smoke-marker hook; defined in `game_phase_transition.cpp` so the
// `<emscripten.h>` include stays out of the header. No-op on native builds.
void notify_phase_changed(entt::registry& reg);

}  // namespace phase_transition_detail

// Erases all eight `GamePhase*Tag` ctx slots and emplaces `PhaseTag`. Use
// when priming the mirror without resetting `phase_timer` (e.g. test setup
// helpers that pin specific timer values). `enter_phase<...>()` calls this
// internally on every transition.
template <typename PhaseTag>
inline void sync_game_phase_tags(entt::registry& reg) {
    phase_transition_detail::erase_all_game_phase_tags(reg);
    reg.ctx().emplace<PhaseTag>();
}

// Transitions the registry to the phase identified by `PhaseTag`: swaps the
// active `GamePhase*Tag` ctx slot, resets `GameState::phase_timer`, and
// fires the WASM smoke-marker title hook. Dispatch happens at the call site
// via the template parameter — no runtime enum compare, no `switch`.
template <typename PhaseTag>
inline void enter_phase(entt::registry& reg) {
    sync_game_phase_tags<PhaseTag>(reg);
    reg.ctx().get<GameState>().phase_timer = 0.0f;
    phase_transition_detail::notify_phase_changed(reg);
}

// Requests a phase transition: erases all `NextPhase*Tag` ctx slots and
// emplaces `NextPhaseTag`. The next `game_state_system` tick reads the tag
// presence and performs the swap (deferred-transition pattern per #482).
template <typename NextPhaseTag>
inline void request_phase_transition(entt::registry& reg) {
    phase_transition_detail::erase_all_next_phase_tags(reg);
    reg.ctx().emplace<NextPhaseTag>();
}

// Drains all eight `NextPhase*Tag` ctx slots. Called by
// `game_state_system` at the bottom of the dispatch block so a stale
// request cannot fire again next frame.
inline void clear_next_phase_tags(entt::registry& reg) {
    phase_transition_detail::erase_all_next_phase_tags(reg);
}

// True iff any `NextPhase*Tag` is present on `reg.ctx()` — i.e. a
// `request_phase_transition<...>` is pending. Replaces the former
// `GameState::transition_pending` boolean.
inline bool is_phase_transition_pending(const entt::registry& reg) {
    const auto& ctx = reg.ctx();
    return ctx.contains<NextPhaseTitleTag>()
        || ctx.contains<NextPhaseLevelSelectTag>()
        || ctx.contains<NextPhasePlayingTag>()
        || ctx.contains<NextPhasePausedTag>()
        || ctx.contains<NextPhaseGameOverTag>()
        || ctx.contains<NextPhaseSongCompleteTag>()
        || ctx.contains<NextPhaseSettingsTag>()
        || ctx.contains<NextPhaseTutorialTag>();
}
