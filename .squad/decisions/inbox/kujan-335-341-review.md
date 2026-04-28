# Review Decision: #335 and #341
**Reviewer:** Kujan  
**Author:** Keaton  
**Commit:** a9ed3fc  
**Date:** 2026-04-27  
**Verdict:** ✅ APPROVED — both issues are closure-ready

---

## #335 — Document or strengthen TestPlayerState planned entity lifecycle contract

**Acceptance criteria check:**

| Criterion | Status |
|-----------|--------|
| Document the planned entity lifetime and validity contract near the component or storage helper | ✅ `LIFETIME CONTRACT` comment added directly above `planned[]` in `app/components/test_player.h` |
| Add or confirm tests for stale planned entities being skipped safely | ✅ Two new `[test_player]` tests: stale entity removed on next tick; live entries retained after stale cleanup |
| Avoid broad casts or unsafe assumptions about entity validity | ✅ Contract comment reinforces `reg.valid()` requirement; `test_player_clean_planned()` implements it with a correct write-cursor in-place filter |

**Verification:**
- `test_player_clean_planned()` is called at line 141 of `test_player_system.cpp`, before any entity access logic — "start of every tick" claim is accurate.
- `is_planned(ghost)` after destroy returns false after the tick as expected — no use-after-free risk.
- All 14 `[test_player]` tests pass (22 assertions).

**No blocking issues.**

---

## #341 — Correct ui-json-to-pod-layout skill guidance for ctx emplace vs insert_or_assign

**Acceptance criteria check:**

| Criterion | Status |
|-----------|--------|
| Update the skill doc to state the correct EnTT ctx guidance | ✅ Table updated; old ambiguous single row replaced with two rows distinguishing `emplace<T>()` (startup one-time, errors if already present) from `insert_or_assign()` (upsert at session-restart/screen-change) |
| Keep this in the Squad-state PR, not the gameplay ECS code PR | ✅ Only `.squad/skills/ui-json-to-pod-layout/SKILL.md` modified — no gameplay code touched |

**Verification:**
- Code comment in example block changed from the misleading `NOT emplace_or_replace` to an accurate screen-change semantics note.
- File content verified via direct inspection — no formatting artifacts in the committed file.

**No blocking issues.**

---

## Full suite

All 2818 assertions pass (842 test cases). Zero regressions.

## Recommendation

Both issues meet all stated acceptance criteria. Coordinator may close #335 and #341.
