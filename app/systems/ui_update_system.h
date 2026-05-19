#pragma once

#include <entt/entt.hpp>

// Entity-driven UI input system (issue #1287, refs #1193 OoS-A).
//
// Hit-tests `view<UiPosition, UiBounds, UiButtonTag>` against the frame's
// pointer-release event read from `InputState` (mouse click or touch lift).
// On a hit, press behavior is selected by the entity's `UiAction*Tag`
// membership, not by an `ActionId` ordinal.
//
// Honours the per-active-phase input-delay table (Game Over uses
// `GAME_OVER_INPUT_DELAY`, Song Complete uses `SONG_COMPLETE_INPUT_DELAY`,
// every other screen uses the shared `UI_ENTRY_DEBOUNCE`) so a click on
// the previous phase's button does not fire on the entry frame of the
// new phase.
void ui_update_system(entt::registry& reg);

// Tutorial-screen "Continue" action (issue #1291). Marks FTUE complete,
// persists the settings entity, and requests the Playing phase. Invoked
// by the `UiActionContinueButtonTag` button path in `ui_update_system.cpp`;
// exposed here so the FTUE end-to-end test in
// `tests/test_game_state_extended.cpp` can drive it directly.
void tutorial_screen_continue(entt::registry& reg);
