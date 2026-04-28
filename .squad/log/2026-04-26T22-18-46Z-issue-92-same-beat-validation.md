# Issue #92: same-beat beatmap validation

**Timestamp:** 2026-04-26T22:18:46Z  
**Squad:** Ralph (implementation), Coordinator (tightening/validation), Kujan (review), Scribe (logging)

## Scope

Resolved the mismatch where the beatmap validator rejected all duplicate `beat` indices even though rhythm charts may author simultaneous obstacles on the same beat.

## Outcome

- `validate_beat_map()` now treats beat indices as non-decreasing rather than strictly increasing.
- Same-beat entries are validated as a group and allowed only when they occupy distinct action slots.
- Duplicate actions, conflicting shape requirements, `low_bar` + `high_bar`, same-lane lane pushes, ComboGate stacked with another shape-bearing obstacle, and deprecated `lane_block` entries are rejected.
- `parse_beat_map()` sorts beat entries deterministically for stable same-beat processing.
- Unknown kind/shape strings now produce parser errors instead of silently defaulting to `shape_gate` / `circle`.
- `rhythm-spec.md` and `beatmap-integration.md` now document the same-beat policy.

## Coordinator tightening

- Added ComboGate conflict handling so same-beat ComboGate + ShapeGate/SplitPath/ComboGate configurations cannot pass when the runtime cannot resolve them safely.
- Skipped shape-gap checks within the same beat so conflicting same-beat shapes report the precise same-beat error instead of an unrelated shape-gap error.
- Added validator tests for valid same-beat multi-action entries, duplicate/conflicting entries, ComboGate edge cases, and same-beat shape conflict error precision.
- Added beat scheduler coverage proving all same-beat action slots spawn in one scheduler tick and advance `next_spawn_idx`.

## Validation

- `git diff --check` passed for the #92 files.
- Native build passed for `shapeshifter_tests`.
- Focused tests passed: 191 assertions in 91 test cases.
- Full native non-benchmark suite passed: 2582 assertions in 782 test cases.

## Review

Kujan approved after the coordinator fixes with no blocking findings.

## GitHub

Comment posted to issue #92.
