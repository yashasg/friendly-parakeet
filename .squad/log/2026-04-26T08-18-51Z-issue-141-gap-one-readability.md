# Issue #141 — Enforce Gap-One Readability in Shipped Beatmaps

**Timestamp:** 2026-04-26T08:18:51Z  
**Status:** APPROVED

## Summary

Gap-one (gap=1) obstacles now enforced with difficulty-stratified readability rules across all 9 shipped beatmaps (3 songs × 3 difficulties).

- **Easy:** No gap=1 obstacles
- **Medium:** gap=1 ≤1 per run, only after 30% progress, same shape+lane for consecutive, away from LanePush/bar ≥2 beats
- **Hard:** gap=1 ≤2 per run, from beat 11+, same family/neighbour rules

## Artifacts

- `tools/level_designer.py`: `clean_gap_one_readability` + policy rules
- `tools/validate_gap_one_readability.py`: Python validator
- `tests/test_shipped_beatmap_gap_one_readability.cpp`: C++ regression test
- `content/beatmaps/*_beatmap.json`: Regenerated × 9

## Validation

- ✅ 2398 C++ assertions / 761 test cases (all passed)
- ✅ 9 beatmap×difficulty combos: zero violations
- ✅ No regression to #125/#134/#135/#137/#138

## Team

- Ralph: Initial implementation
- Coordinator: Finalization + Catch2 alignment
- Kujan: Review & Approval

## Next

GitHub comment posted. Ready for merge.
