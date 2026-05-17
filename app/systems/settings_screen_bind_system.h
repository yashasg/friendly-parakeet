#pragma once

#include <entt/entt.hpp>

// Settings screen dynamic-text + toggle-state binder
// (issue #1295, refs #1287, #1193 OoS-A).
//
// Writes per-frame Settings UI data into the entities spawned by
// `spawn_settings_screen()` (codegen, see
// `app/systems/generated/settings_screen.cpp`):
//
//   * AudioOffsetDisplay label  → "%+d ms" from `SettingsState::audio_offset_ms`
//   * HapticsToggle      label  → "[X] HAPTICS: ON" / "[ ] HAPTICS: OFF"
//   * HapticsToggle      toggle → `UiToggleState{haptics_enabled}`
//   * ReduceMotionToggle label  → "[X] MOTION: ON" / "[ ] MOTION: OFF"
//                                 (display is inverted — MOTION ON when
//                                  `reduce_motion` is false)
//   * ReduceMotionToggle toggle → `UiToggleState{!reduce_motion}`
//
// Slot entities are identified by their canonical (x, y) position baked
// from `content/ui/screens/settings.rgl`; the bind table is per-slot
// rows of (x, y, write fn) — no `switch` over a discriminator.
//
// Runs after `screen_lifecycle_system` so the Settings entity set is
// guaranteed to exist whenever `GamePhaseSettingsTag` is the active
// phase; trivially no-ops on every other phase (the view is empty).
void settings_screen_bind_system(entt::registry& reg);
