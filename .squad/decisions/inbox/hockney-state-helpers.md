# Decision: HighScoreState/SettingsState helpers live in persistence namespaces

**Date:** 2026-04-27  
**Issue:** #286  
**Author:** Hockney

## Decision
Business logic helpers for `SettingsState` and `HighScoreState` are placed as free functions in `namespace settings` (settings_persistence.h) and `namespace high_score` (high_score_persistence.h) respectively, NOT on the structs as methods.

## Rationale
- Keeps structs as plain data containers per project convention.
- Persistence namespaces already own the helpers for clamping/saving/loading; helper functions are a natural extension.
- Avoids coupling data layout to behaviour, making it easier to evolve ownership independently.

## Affected symbols
- `settings::audio_offset_seconds(const SettingsState&)`
- `settings::ftue_complete(const SettingsState&)`
- `high_score::make_key_str/make_key_hash/get_score/get_score_by_hash/set_score/set_score_by_hash/ensure_entry/get_current_high_score`
