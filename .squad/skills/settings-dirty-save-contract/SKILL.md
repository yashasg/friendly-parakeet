# Skill: settings-dirty-save-contract

## Problem
Runtime settings mutate in UI but persistence state exists separately in registry context. Without a save contract, changes are lost on restart.

## Pattern
Use a shared helper that owns persistence state transitions:
1. Emplace `SettingsDirtyTag` on the SettingsTag entity before save attempt.
2. If `SettingsPersistence::path` is empty, set `last_save = PathUnavailable` and keep the tag.
3. Otherwise call `settings::save_settings(...)`.
4. Remove `SettingsDirtyTag` only on success; keep it on failure for retry.

The former `bool dirty` column on `SettingsPersistence` / `HighScorePersistence` was eradicated per Fabian relational normalization (#1203): "needs save" is now expressed as the presence of an existence tag (`SettingsDirtyTag` on the SettingsTag entity, `HighScoreDirtyTag` on `registry.ctx()`).

## Integration Point
Call `settings::mark_dirty_and_save(reg, state)` exactly where settings mutate (currently `settings_screen_controller`, `tutorial_screen_controller`, and `game_state_routing`) so state is persisted at mutation time.

## Regression Guard
Keep tests that assert:
- successful save removes `SettingsDirtyTag` and round-trips settings values;
- path-unavailable save keeps `SettingsDirtyTag` and exposes `last_save.status`.
