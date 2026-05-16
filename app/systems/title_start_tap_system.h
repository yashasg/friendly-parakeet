#pragma once

#include <entt/entt.hpp>

// Title-screen "tap anywhere to start" gesture (issue #1294 migration).
//
// Replaces the legacy `title_screen_controller`'s
// `read_title_pointer_release` + `title_tap_in_dead_zone` helpers — which
// were derived from raygui layout-state accessors — with an entity-driven
// hit-test. The rule preserved across the migration:
//
//   * If the active phase is GamePhaseTitleTag,
//   * AND `InputState::click || InputState::touch_up` fires this frame,
//   * AND `gs.phase_timer > UI_ENTRY_DEBOUNCE` (open-tap suppression),
//   * AND the press did NOT hit ANY `UiButtonTag + TitleScreenTag` entity
//     bounds (including `UiHiddenOnWebTag` entities, whose bounds remain
//     a dead-zone on Web per #511),
//   * THEN request the LevelSelect phase transition.
//
// Per Fabian Principle 1 the dead-zone set is computed from existential
// presence (button entities on the screen) rather than a hand-curated list
// of bounds accessors. Adding / moving / removing a Title-screen button
// updates the dead-zone for free — the `.rgl` is the single source.
void title_start_tap_system(entt::registry& reg);
