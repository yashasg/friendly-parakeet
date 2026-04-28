# Kujan Review Decision — #346 (spawn_ui_elements extraction + malformed JSON regression tests)

**Date:** 2026-04-27  
**Reviewer:** Kujan  
**Author:** Baer  
**Commits:** 710ff34, ff96be8

## Verdict: ✅ APPROVED — closure-ready

## What Was Reviewed

- `spawn_ui_elements()` extracted from `static` scope in `ui_navigation_system.cpp` into `ui_loader.cpp/h` as a non-static free function.
- `json_color_rl` null-safety guard added (`is_array() && size() >= 3` check).
- Dead statics removed from `ui_navigation_system.cpp`.
- `tests/test_ui_spawn_malformed.cpp`: 12 new regression tests covering all four catch branches and positive paths.

## Evidence

- Build: zero warnings (`-Wall -Wextra -Werror`), zero errors.
- Test suite: 854 test cases / 2845 assertions — all pass.
- `[issue346]` filter: 12 test cases / 27 assertions — all pass.
- Render system uses multi-component queries (`view<UIElementTag, UIText, Position>` etc.) — confirmed safe against orphan entities.
- `destroy_ui_elements()` uses `view<UIElementTag>()` — all spawned entities (including orphans) are cleaned on screen transition.

## Non-Blocking Observation (follow-on recommended)

`spawn_ui_elements()` does not `continue` or warn on unrecognized element types (e.g., `stat_table`, `card_list`, `line`, `burnout_meter`). These create orphan entities with only `UIElementTag` + `Position`. This is **pre-existing behavior** and currently benign, but creates a silent footgun if future types are added to `kSupportedTypes` without a spawn branch.

**Recommended follow-on:** Add an `else` branch at the end of the type dispatch to `reg.destroy(e); continue;` with a `stderr` warning for unknown types.

## No Blockers

No revision required. #346 is closure-ready.
