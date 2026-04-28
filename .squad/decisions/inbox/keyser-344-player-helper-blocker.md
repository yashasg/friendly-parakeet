# Decision: make_player routes through create_player_entity

**Date:** 2026-05-18  
**Author:** Keyser  
**Issue:** #344 blocker

## Decision

`make_player` in `tests/test_helpers.h` now routes through `create_player_entity(reg)`, identical to `make_rhythm_player`. This is the single canonical path for constructing a production-equivalent player entity in tests.

## Rationale

`create_player_entity` is the single source of truth for player component layout and initial values (Hexagon shape, center lane, Grounded vertical state). Any test helper that raw-emplaces the same components creates a drift risk — changes to the archetype must be reflected in both places. Routing through the factory eliminates that drift.

## Implications for test authors

- `make_player` now starts the player as **Hexagon** (not Circle).
- Tests that need a specific non-production shape (e.g., Circle for a shape-gate matching test) must **explicitly set `reg.get<PlayerShape>(p).current = Shape::Circle`** at the callsite. This makes the test's intent clear and self-documenting.
- There is no longer a "Circle default" test player. If a future test genuinely requires a non-production player setup, create a clearly named helper (e.g., `make_circle_player`) rather than relying on struct defaults.
