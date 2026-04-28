# Kujan — Review Decision: UI ECS Batch (#337, #338, #339, #343)

**Date:** 2026-05-18  
**Reviewer:** Kujan  
**Author:** Keyser (decision record) / Keaton (code, landed in `fdefeb1`)  
**Requested by:** yashasg

---

## Verdict: ✅ APPROVED

All four issues are closure-ready. One non-blocking hardening gap noted for #338.

---

## Issue-by-Issue Findings

### #337 — UIActiveCache startup initialization

**Status: CONFIRMED IMPLEMENTED**

- `app/game_loop.cpp:118` — `reg.ctx().emplace<UIActiveCache>();` ✅
- `tests/test_helpers.h:53` — `reg.ctx().emplace<UIActiveCache>();` ✅
- `active_tag_system.cpp` — `ctx().get<UIActiveCache>()` (hard crash if absent); lazy `find/emplace` removed ✅
- Diff in `fdefeb1` directly confirmed the removal.
- Existing `[hit_test][active_tag]` tests exercise both entry points through `make_registry()`. All pass.

**Closure-ready.** ✅

---

### #338 — Safe JSON access in UI spawn path

**Status: CONFIRMED IMPLEMENTED**

All `operator[]` on required fields in `spawn_ui_elements()` replaced with `.at()` + `try/catch`:

| Element type | Error class | Behavior |
|---|---|---|
| text/text_dynamic animation | `.at()` on sub-keys | `[WARN]` stderr, entity preserved |
| button required colors | `.at("bg_color")` etc. | `[WARN]` + `reg.destroy(e)` + `continue` |
| shape required color | `.at("color")` | `[WARN]` + `reg.destroy(e)` + `continue` |

Decision record's stated behavior matches code exactly.

**Non-blocking gap:** The three `catch` branches have zero test coverage. A future refactor that silently removes them would pass the full suite undetected. Recommend a hardening ticket.

**Closure-ready.** ✅ (with hardening recommendation)

---

### #339 — UIState.current hashing: keep as std::string

**Status: CONFIRMED DOCUMENTED — NO CODE CHANGE REQUIRED**

- `UIState.current` confirmed `std::string` in `app/components/ui_state.h`.
- Rationale is architecturally sound: comparison occurs at screen-load phase-transition boundaries only, not per-frame.
- Per-frame element lookup already uses `entt::hashed_string` (#312); screen names are outside the hot path.
- SSO handles the short bounded set of screen name strings.
- Migrating to `entt::id_type` would sacrifice debuggability for zero measurable benefit.

**Closure-ready.** ✅

---

### #343 — Dynamic UI text allocation: no cache

**Status: CONFIRMED DOCUMENTED — NO CODE CHANGE REQUIRED**

- `resolve_ui_dynamic_text()` called per-frame per `UIDynamicText` entity in render system (~2–5 entities).
- Strings < 20 chars; libc++ SSO threshold ~15–22 chars. Most are stack-local; worst case 300 small ops/sec.
- Cache invalidation complexity (compare-each-frame or event-wiring per source) exceeds the measurable benefit.
- `ScoreState.score`, `SettingsState.haptics_enabled`, `GameOverState.reason` change at game-event boundaries, making a cache neither simpler nor faster.

**Closure-ready.** ✅

---

## Test Suite

**2808 assertions / 840 test cases — all pass** (independently verified via `./run.sh test`).

---

## Hardening Recommendation (non-blocking)

Open a follow-up ticket to add tests for `spawn_ui_elements()` malformed-JSON error paths:
1. Animation block with missing `speed` sub-key → `[WARN]`, entity preserved, no `UIAnimation` component
2. Button missing `bg_color` → `[WARN]`, entity destroyed
3. Shape missing `color` → `[WARN]`, entity destroyed

These are defensive paths; their absence is not a correctness issue for the happy path but is a regression risk for future refactors.
