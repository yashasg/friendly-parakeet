#pragma once

#include <entt/entt.hpp>

// Entity-driven UI input system (issue #1287, refs #1193 OoS-A).
//
// Hit-tests `view<UiPosition, UiBounds, OnPress, UiButtonTag>` against the
// frame's pointer-release event read from `InputState` (mouse click or
// touch lift). On a hit, dispatches the entity's `OnPress::action` through
// a per-`ActionId` function-pointer table — no `switch` on the
// discriminator (Fabian Principle 1; `ActionId` is in the enum allowlist
// specifically as a lookup-key index).
//
// Migration boundary: until all 8 screens migrate to the entity-driven
// path, only screens whose buttons are actually spawned participate;
// un-migrated screens continue to dispatch via their legacy
// `screen_controllers/*` cpp. See #1287 for the per-screen sub-issue
// ladder.
//
// Honours the `gs.phase_timer > UI_ENTRY_DEBOUNCE` debounce that legacy
// controllers apply so a click on the previous phase's button does not
// fire on the entry frame of the new phase.
void ui_update_system(entt::registry& reg);
