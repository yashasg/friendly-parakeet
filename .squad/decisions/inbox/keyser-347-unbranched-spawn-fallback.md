# Decision: Destroy unbranched supported element entities in spawn_ui_elements (#347)

**Author:** Keyser  
**Date:** 2026-05-18  
**Issue:** #347

## Decision

Added an explicit `else if (is_supported_type(type))` fallback at the end of the
`spawn_ui_elements()` if/else chain. Any supported-but-unimplemented element type
(`stat_table`, `card_list`, `line`, `burnout_meter`, `shape_button_row`, `button_row`)
now has its base entity (`UIElementTag` + `Position`) destroyed and the loop continues.
A final `else` branch handles genuinely unknown types with a `[WARN]` stderr log and
also destroys the entity.

## Rationale

Prior to this fix, these types left orphan entities in the ECS registry — entities that
held `UIElementTag` + `Position` but no renderable component. While benign now, orphans
are misleading noise (spurious hits in `UIElementTag` views) and a maintenance hazard
as renderable components are added for these types later.

The fallback uses `is_supported_type()` which already maintains the canonical list, so
adding a spawn branch for any of these types in the future will automatically bypass
the fallback — no separate cleanup needed.

## Coverage

8 new regression tests added to `tests/test_ui_spawn_malformed.cpp` tagged `[issue347]`.
Full suite: 2854 assertions, all pass.

## Closure Readiness

✅ Issue #347 is ready to close.
