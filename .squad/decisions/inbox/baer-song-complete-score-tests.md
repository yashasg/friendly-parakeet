# Decision: Song-Complete Score/High-Score Render Regression Tests

**Date:** 2026-05-27  
**Author:** Baer (Test Engineer)  
**Triggered by:** Bug report — "when the song completes score and highscore dont render"

## Coverage gaps identified

Existing tests verified `score.high_score` is updated on transition and that all JSON sources *resolve* (with zero values). They did NOT pin:

1. That `score.score` is **retained** (not zeroed) when `enter_song_complete` fires.
2. That the sources resolve to **non-empty, correct strings** using non-zero gameplay values.
3. That `song_complete.json` binds to the **exact** source strings `"ScoreState.score"` and `"ScoreState.high_score"` (not `displayed_score` or a wrong key).

## Tests added

### `tests/test_game_state_extended.cpp` (3 new cases, tag `[song_complete]`)

| Test | Guards against |
|------|---------------|
| `score.score is retained (not zeroed) after enter_song_complete` | `enter_song_complete` accidentally resetting `ScoreState` |
| `both score and high_score resolve to non-empty strings after transition` | Resolver returning nullopt or wrong value when score > high_score |
| `score is visible even when it does not set a new high score` | Resolver silenced when score < existing high_score |

### `tests/test_ui_source_resolver.cpp` (3 new cases, tag `[song_complete]`)

| Test | Guards against |
|------|---------------|
| `song_complete.json: score element declares source ScoreState.score` | JSON source field rename or typo |
| `song_complete.json: high_score element declares source ScoreState.high_score` | JSON source field rename or typo |
| `ScoreState.score source formats to decimal string` | Resolver returning empty/nullopt for non-zero values |

## Render-path gap (open, for McManus/Redfoot)

The `draw_hud` function renders score/high_score via `find_el` + `text_draw_number` only for `Gameplay` and `Paused` phases. For `SongComplete` the ECS `UIText`/`UIDynamicText` path is used instead (correct). However, the dynamic text elements in `song_complete.json` do **not** set `"align": "center"` — the text spawns left-aligned at `x_n: 0.5`, which may cause visual offset. This is a layout concern, not a data concern; it is outside scope for these regression tests.
