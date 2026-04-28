# Keyser — UI ECS Batch Decision Record
**Date:** 2026-05-18  
**Issues:** #337, #338, #339, #343  
**Author:** Keyser (Lead Architect)

---

## #337 — UIActiveCache startup initialization

**Decision:** IMPLEMENTED (via `fdefeb1`, Keaton's migration commit).

`UIActiveCache` is now emplaced in:
- `app/game_loop.cpp` — alongside all other ctx singletons at startup
- `tests/test_helpers.h` `make_registry()` — so all test registries have it

`active_tag_system.cpp` now uses `ctx().get<UIActiveCache>()` (hard fail if missing) instead of lazy `find/emplace`. This makes the initialization contract explicit and keeps the hot update path deterministic.

**Status:** Closure-ready. All 840 test cases pass.

---

## #338 — Safe JSON access in UI element spawn path

**Decision:** IMPLEMENTED (via `fdefeb1`, Keaton's migration commit).

In `spawn_ui_elements()` (`ui_navigation_system.cpp`):
- Animation block: `el.at("animation").at("speed")` etc., wrapped in try/catch → `[WARN]` on schema error
- Button required colors (`bg_color`, `border_color`, `text_color`): `el.at(...)`, outer try/catch destroys entity on schema error
- Shape color (`color`): `el.at("color")`, try/catch destroys entity on schema error

Missing animation keys now produce `[WARN]` stderr output instead of asserting. Missing required button/shape fields destroy the partially-constructed entity and log a warning.

**Status:** Closure-ready. All 840 test cases pass.

---

## #339 — UIState.current hashing: keep as std::string

**Decision:** NO MIGRATION — keep `UIState.current` as `std::string`.

**Rationale:**
1. `current` is compared once at screen-load time (phase transition boundary), not per-frame. Cost is negligible.
2. Screen names are a small bounded set of short strings (`"title"`, `"gameplay"`, `"game_over"`, etc.) — SSO keeps them stack-local.
3. Migrating to `entt::id_type` would require adding a separate debug string field to preserve readability in logs and RAII tools.
4. Element IDs are hashed for O(1) per-frame map lookup (#312). Screen names are never looked up per-frame — only used for idempotent load prevention.
5. The ECS principle to "use typed/hash IDs consistently" applies to hot-path lookups. Load-time singleton comparisons are outside that principle's scope.

**Status:** Closure-ready with rationale. No code change required.

---

## #343 — Dynamic UI text allocation: document, no cache

**Decision:** NO CACHE — allocation is negligible, caching adds net complexity.

**Measurement:**
- Dynamic text entities per screen: ~2–5 (`score`, `chain_count`, `haptics_button`, `death_reason`).
- Strings are short (< 20 chars). C++ SSO threshold is ~15–22 chars (libc++/MSVC); most resolve strings fit within SSO and don't heap-allocate at all.
- Worst case at 60fps × 5 entities = ~300 small string operations/sec. With SSO, most are stack-only.
- No measurable allocation pressure: the rendering path spends its time in raylib draw calls, not string resolution.

**Cache complexity cost:**
- Needs a `std::string resolved_text` field per `UIDynamicText` entity (component bloat).
- Needs an invalidation boundary: either poll-compare each frame (same cost as resolution) or wire a state-change listener per source.
- Sources (`ScoreState.score`, `SettingsState.haptics_enabled`, `GameOverState.reason`) change at game-event boundaries, not per-frame — but detecting the change requires either comparison or event wiring.
- Net result: cache adds ~2x complexity for zero measurable benefit.

**Status:** Closure-ready with rationale. No code change required.

---

## Summary

| Issue | Action | Status |
|-------|--------|--------|
| #337  | `UIActiveCache` initialized at startup, lazy creation removed | ✅ Implemented |
| #338  | `operator[]` → `.at()` + try/catch in spawn path | ✅ Implemented |
| #339  | Keep `UIState.current` as `std::string` (rationale documented) | ✅ Documented |
| #343  | No cache; SSO makes allocation negligible (rationale documented) | ✅ Documented |
