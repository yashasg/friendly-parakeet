# EnTT Core_Functionalities Leverage — Audit Conclusions

**Author:** Keyser  
**Date:** 2026-05-18  
**Status:** DOCUMENTED — two issues filed, P2 implemented

## What Was Audited

`docs/entt/Core_Functionalities.md` vs. active codebase — looking for actionable new EnTT features beyond the prior ECS structural audit (decisions.md: "EnTT ECS Audit Findings 2026-05-17").

## New Issues Filed

| Issue | Priority | Feature | File | Status |
|-------|----------|---------|------|--------|
| [#310](https://github.com/yashasg/friendly-parakeet/issues/310) | P2 | `entt::hashed_string` for UI source resolver string dispatch | `app/systems/ui_source_resolver.cpp` | **IMPLEMENTED** in commit `4f4574f` |
| [#314](https://github.com/yashasg/friendly-parakeet/issues/314) | P3 | `entt::enum_as_bitmask` for `ActiveInPhase` phase-mask type | `app/components/input_events.h`, `game_state.h` | Open — owner: McManus |

## P2 — #310 Implemented

`resolve_ui_int_source` (15 branches) and `resolve_ui_dynamic_text` (5 branches) in `ui_source_resolver.cpp` converted from sequential `if (source == "...")` string comparisons to `switch(entt::hashed_string::value(source.data()))` with `_hs` UDL case labels. Single FNV-1a hash per dispatch replaces worst-case N×string_length comparisons. Sources are JSON-parsed `std::string` fields — `data()` is null-terminated, safe for `hashed_string::value()`.

## P3 — #314 Deferred

`ActiveInPhase::phase_mask (uint8_t)` uses manual helpers `phase_bit()` / `phase_active()` for bit manipulation on `GamePhase` indices. A dedicated `GamePhaseBit` enum (power-of-2 values, `_entt_enum_as_bitmask` sentinel) would make the mask type-safe and idiomatic. Current helpers are correct; this is ergonomic only. Deferred to McManus.

## Features Reviewed — No Issue

| Feature | Rationale for No Issue |
|---------|----------------------|
| `entt::monostate` | `reg.ctx()` is superior (registry-scoped, testable). Not worth migrating. |
| `entt::any` | Typed `ctx()` pattern already covers all use-cases cleanly. |
| `entt::tag<"name"_hs>` | Existing empty tag structs are more readable. No value in retrofitting. |
| `entt::compressed_pair` | Internal library utility; no game-side benefit. |
| `entt::allocate_unique` | No custom allocators in use. |
| `entt::overloaded` | No `std::variant` pattern identified in codebase. |
| `entt::type_name<T>()` | `magic_enum` already covers enum value names; no component-type debug logging gap. |
| `entt::fast_mod` | Hot-path moduli found (lane `% 3`) are not powers of 2 — `fast_mod` requires power-of-2. |
| `entt::ident` / `entt::family` | No compile-time or runtime type sequencing need identified. |
| `entt::integral_constant` tags | Empty structs are fine; `entt::tag<"x"_hs>` only saves a struct definition. |

## Guidance for Team

- **New UI data-binding sources**: Add cases to the `switch` in `ui_source_resolver.cpp`. Follow `"Context.field"_hs` naming convention. The compiler will flag duplicate case values (FNV-1a collision guard).
- **`entt::hashed_string` null-termination assumption**: `source.data()` passed to `hashed_string::value()` is safe only when `source` comes from a `std::string` or literal. Do NOT use on mid-buffer `string_view` slices without a local copy.
- **entt::dispatcher**: Already the active migration target for `EventQueue` (decisions.md). That migration supersedes Core_Functionalities findings for input.
