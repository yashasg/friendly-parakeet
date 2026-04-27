# Issue #72: Reconcile LaneBlock and LanePush obstacle taxonomy

**Timestamp:** 2026-04-26T10:51:51Z  
**Squad:** Ralph (implementation), Coordinator (tightening/validation), Kujan (review), Scribe (logging)

## Scope

Resolved drift between legacy `lane_block` naming and shipping `lane_push_left` / `lane_push_right` lane obstacle semantics.

## Outcome

- Shipping authored lane obstacle taxonomy is now explicitly `lane_push_left` and `lane_push_right`.
- `LanePushLeft` / `LanePushRight` are documented as passive lane pushes:
  - auto-push only if player is on the same lane and not already moving
  - edge pushes are no-ops
  - 0 points
  - no energy effect
- `lane_block` / `ObstacleKind::LaneBlock` is deprecated, legacy, and non-shipping.
- Parser/runtime compatibility for `lane_block` remains for old fixtures/prototype paths.
- Shipping validation, tooling, generators, and editor guidance reject or avoid `lane_block`.

## Coordinator tightening

- Marked remaining lane-block compatibility test names/comments as legacy.
- Verified shipped beatmaps have no `lane_block` references.
- Confirmed `validate_beat_map()` rejects legacy LaneBlock while `parse_beat_map()` still accepts it for migration.
- Confirmed prototype random spawning converts the legacy LaneBlock selector into LanePushLeft/Right.

## Validation

- `git diff --check` on taxonomy files
- CMake configure in verify directory
- `cmake --build <verify-dir> --target shapeshifter shapeshifter_tests`
- `<verify-dir>/shapeshifter_tests "[validate],[taxonomy],[beat_scheduler],[rhythm][beatmap]"`
- `python3 tools/validate_difficulty_ramp.py`
- `python3 tools/validate_beatmap_bars.py`
- `rg "lane_block|LaneBlock" content/beatmaps`
- `<verify-dir>/shapeshifter_tests "~[bench]"`

## Review

Kujan approved with no changes requested.

## GitHub

Comment posted to issue #72.
