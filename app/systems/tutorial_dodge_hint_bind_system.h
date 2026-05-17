#pragma once

#include <entt/entt.hpp>

// Binds platform-correct text into the Tutorial screen's DodgeHint label
// entity (issue #1291, refs #1287 / #1193 OoS-A).
//
// The .rgl emits a single `DodgeHint` label with empty text (a dynamic-text
// slot). Runtime selects keyboard vs swipe copy per platform via
// `web_input_touch_capable`; before #513 the copy was hard-coded behind
// `#ifdef PLATFORM_HAS_KEYBOARD`, which produced incorrect copy on
// touch-only Web browsers. The runtime selector now lives in
// `tutorial_dodge_hint_text()` (`app/util/tutorial_dodge_hint.h`).
//
// Runs after `screen_lifecycle_system` (which spawns the entity) and
// before `ui_render_system` (which reads `UiLabel::text`). No-op when the
// Tutorial screen entities are absent (i.e. any other phase).
void tutorial_dodge_hint_bind_system(entt::registry& reg);
