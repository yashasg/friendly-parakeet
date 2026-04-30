# Skill: settings-dirty-save-contract

## Problem
Runtime settings mutate in UI but persistence state exists separately in registry context. Without a save contract, changes are lost on restart.

## Pattern
Use a shared helper that owns persistence state transitions:
1. Set `SettingsPersistence.dirty = true` before save attempt.
2. If `path` is empty, set `last_save = PathUnavailable` and keep dirty.
3. Otherwise call `settings::save_settings(...)`.
4. Clear dirty only on success; keep dirty on failure for retry.

## Integration Point
Call the helper exactly where settings mutate (currently `settings_screen_controller`) so state is persisted at mutation time.

## Regression Guard
Keep tests that assert:
- successful save clears dirty and round-trips settings values;
- path-unavailable save keeps dirty and exposes `last_save.status`.
