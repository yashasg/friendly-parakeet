# EnTT Core Functionalities — Implementation Decisions

**Author:** Keaton (C++ Performance Engineer)
**Date:** 2026
**Status:** GUIDANCE

## Summary

Audited `docs/entt/Core_Functionalities.md` against the full `app/` codebase. Filed GitHub issues #308–#313 tagged `ecs_refactor` under TestFlight milestone. P1 fixes landed in commit 7fc569a.

## Decisions

### 1. Static vector pattern is the standard for deferred-destroy loops (CONFIRMED)
`static std::vector<entt::entity>; .clear()` is the canonical pattern for collect-then-destroy loops in hot systems. `cleanup_system.cpp` is the reference implementation. `lifetime_system.cpp` now matches.

### 2. `ctx().find<>()` must be hoisted above all entity loops
Any `reg.ctx().find<T>()` or `reg.ctx().get<T>()` call inside a `view.each()` loop is a pattern violation. The lookup is O(1) but the null-check guard inside the loop is noise. Hoist once, use the pointer throughout. Applies to all future system authors.

### 3. `entt::enum_as_bitmask` is the right replacement for `ActiveInPhase.phase_mask` (PENDING)
The `GamePhase` enum + `phase_bit()` + `phase_active()` manual bitmask pattern should be replaced with `_entt_enum_as_bitmask`. **Blocker:** `GamePhase` values 0–5 must become powers-of-two 0x01–0x20. Any serialized or raw-cast usage must be audited first. Do not land until all GamePhase integer usages are confirmed safe.

### 4. `entt::hashed_string` is the right key type for the UI element lookup map (PENDING)
`find_el()` in `ui_render_system.cpp` should be replaced with a pre-built `std::unordered_map<entt::hashed_string::hash_type, const json*>` at `UIState::load_screen()`. Lookup keys are compile-time string literals that become `"id"_hs` constants. FNV-1a collision risk is negligible for ~10 element IDs per screen.

### 5. `entt::monostate` is NOT adopted — `reg.ctx()` remains the singleton standard
The project uses `reg.ctx().emplace<T>()` consistently for all game singletons. `entt::monostate` is global and bypasses registry lifetime management. Decision: never adopt for game-owned state.

### 6. No new EnTT Core utilities are needed for the hot path
`entt::any`, `allocate_unique`, `y_combinator`, `iota_iterator`, `type_index`, `family`, `ident`, `compressed_pair`, `input_iterator_pointer` — none apply to the current codebase patterns.

## What Needs Sign-Off Before #311 Lands

The `entt::enum_as_bitmask` change for `GamePhase` requires confirming:
- No serialized `GamePhase` integer values in beatmap JSON or settings files
- No network/IPC protocol using raw GamePhase integers
- All `static_cast<uint8_t>(phase)` sites are safe after value reassignment

Recommend Kujan or McManus does a call-site audit before implementation starts.
