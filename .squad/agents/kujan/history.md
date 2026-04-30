# Kujan — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Reviewer
- **Joined:** 2026-04-26T02:12:00.632Z

## Learnings

### 2026-05-XX — Review: Pause Screen UI Text Fix (Keaton)

**Verdict:** ❌ REJECT — structural fix present but numeric values miss Redfoot AC

**Files reviewed:** `app/ui/generated/paused_layout.h`, `content/ui/screens/paused.rgl`, `app/ui/screen_controllers/paused_screen_controller.cpp/.h`, `app/systems/ui_render_system.cpp`

**What was correct:**
- `PausedLayout_DrawCenteredLabel()` helper added with identical save/restore pattern to `SongCompleteLayout_DrawCenteredLabel` — correct structural approach
- Helper is used for all three text labels in `PausedLayout_Render()`
- Buttons (RESUME 160, 620, 400×100; MAIN MENU 160, 820, 400×100) unchanged
- No legacy paths: no `ui_loader`, `ui_button_spawner`, `spawn_ui_elements`, no `DrawText`, no JSON adapters
- `ui_render_system.cpp` unchanged — letterbox hit-mapping preserved
- Build: zero warnings; tests: 2148 assertions, 756 cases — all pass

**Blocking failures vs. Redfoot AC:**
| AC | Required | Actual | Result |
|---|---|---|---|
| "PAUSED" font size | 56 | 48 | ❌ |
| "PAUSED" rect x,w,h | ~90, 420, 540, 80 | 160, 430, 400, 72 | ❌ |
| "TAP RESUME…" font size | 24 | 22 | ❌ |
| "TAP RESUME…" rect width | ≥540 | 500 | ❌ |
| "OR RETURN…" font size | 24 | 22 | ❌ |
| "OR RETURN…" rect width | ≥540 | 500 | ❌ |

**Pattern learned:** When AC specifies bold numeric values (font size, rect dimensions), those are exact requirements. The implementer used visually-similar but non-conforming values (48 not 56, 22 not 24, 500 not 540). The Redfoot spec also uses "≥540" for instruction widths — this is a floor, not a suggestion.

**Lockout:** Keaton is locked out of the follow-up revision per reviewer protocol. Recommend a non-Keaton agent (e.g., the agent who landed the Song Complete fix) own the correction.

**Key AC source:** `.squad/decisions/inbox/redfoot-pause-screen-text-fix.md`

### Pause Screen — Fenster Revision (Final Review, APPROVED)

**Date:** 2026-05-XX
**Implementer:** Fenster (non-Keaton per lockout protocol)
**Prior rejection:** Keaton's values missed on all 6 numeric AC items (sizes 48/22 not 56/24, widths 400/500 not 540, wrong rect y/x origins).

**Fenster's correction passed all AC items:**
- "PAUSED": `(90, 420, 540, 80)`, size 56 ✅
- "TAP RESUME TO CONTINUE": `(90, 540, 540, 36)`, size 24 ✅
- "OR RETURN TO MAIN MENU": `(90, 760, 540, 36)`, size 24 ✅
- `paused.rgl` geometry mirrors header exactly ✅
- `PausedLayout_DrawCenteredLabel()` save/restore pattern identical to `SongCompleteLayout_DrawCenteredLabel` ✅
- Buttons (RESUME, MAIN MENU) geometry and dispatch unchanged ✅
- No forbidden legacy paths ✅
- Build: zero warnings; tests: 2148 assertions, 771 cases — all pass ✅

**Pattern reinforced:** When rejecting on numeric AC failures and assigning to a non-original-author reviser, the reviser should apply the exact numbers from the AC spec and mirror them in both the `.h` and the `.rgl`. Fenster did this correctly.

**Verdict:** ✅ APPROVED

## 2026-04-29T09:55:21Z — Reviewer: Song Complete & Pause Screen Text Fixes

**Session:** UI Layout Fixes — Song Complete & Pause Screen Text Readability
**Scope:** Reviewed two parallel UI text fixes (Keyser on Song Complete, Keaton on Pause Screen).

### Song Complete Review — Initial Attempt (Keyser, Rejected)

**Verdict:** ❌ REJECTED

**Reason:** No visible active artifacts demonstrated fix. Default GuiLabel still in use with no centered-label helper added. Song Complete title/status text acceptance criteria unmet.

**Lockout applied:** Keyser locked out for revision cycle. Next reviser must be different.

**Related:** `.squad/orchestration-log/2026-04-29T09:55:21Z-kujan-song-complete.md`

### Pause Screen Review — Initial Attempt (Keaton, Rejected)

**Verdict:** ❌ REJECTED — Numeric AC failures on all 6 items

**Blocking failures vs. Redfoot AC:**

| Label | AC Requirement | Actual | Result |
|---|---|---|---|
| "PAUSED" font size | **56** | 48 | ❌ |
| "PAUSED" rect | ~(90, 420, 540, 80) | (160, 430, 400, 72) | ❌ |
| "TAP RESUME TO CONTINUE" font size | **24** | 22 | ❌ |
| "TAP RESUME TO CONTINUE" rect width | **≥540** | 500 | ❌ |
| "OR RETURN TO MAIN MENU" font size | **24** | 22 | ❌ |
| "OR RETURN TO MAIN MENU" rect width | **≥540** | 500 | ❌ |

**Structural quality:** ✅ Helper pattern correct (save/restore), buttons unchanged, no legacy paths reintroduced, build/tests pass.

**Required corrections:** Update three label call-site arguments: "PAUSED" size→56 and rect→(90,420,540,80); both instruction labels size→24 and width→540. Update `content/ui/screens/paused.rgl` to match.

**Lockout applied:** Keaton locked out for revision cycle. Recommended: assign to agent who landed Song Complete fix (already owns helper pattern).

**Related:** `.squad/orchestration-log/2026-04-29T09:55:21Z-keaton-pause.md`

### Pause Screen Review — Fenster Revision (Approved)

**Verdict:** ✅ APPROVED

**Implementation:** Fenster (non-Keaton per lockout protocol) corrected only the three pause label call-site bounds/sizes in `app/ui/generated/paused_layout.h` and mirrored them in `content/ui/screens/paused.rgl`. Buttons/actions unchanged.

**AC verification:** All requirements met exactly:
- "PAUSED": (x=90, y=420, w=540, h=80), size 56 ✅
- "TAP RESUME TO CONTINUE": (x=90, y=540, w=540, h=36), size 24 ✅
- "OR RETURN TO MAIN MENU": (x=90, y=760, w=540, h=36), size 24 ✅
- `PausedLayout_DrawCenteredLabel()` matches `SongCompleteLayout_DrawCenteredLabel` pattern ✅
- Buttons (RESUME, MAIN MENU) untouched ✅
- No legacy UI paths ✅
- Build: zero warnings; tests: 2148 assertions, 771 cases — all pass ✅

**Sign-off:** Pause screen text readability fix complete. All numeric and structural criteria met. Ready to merge.

**Related:**
- `.squad/orchestration-log/2026-04-29T09:55:21Z-fenster.md`
- Session log: `.squad/log/2026-04-29T09:55:21Z-ui-layout-fixes.md`

## Learnings — 2026-04-29: Archetypes Removal Review

**PR scope:** Removal of `app/archetypes/` shim folder; canonicalization on `app/entities/` factories.

**Verdict:** APPROVED.

**Key patterns confirmed:**
- Directory physically absent; grep sweep confirmed zero residual references across app/, tests/, CMakeLists.txt, design-docs/.
- ARCHETYPE_SOURCES CMake glob removed; ENTITY_SOURCES glob (`app/entities/*.cpp`) is the live wiring.
- Test file (`test_player_archetype.cpp`) retains `[archetype]` tag (acceptable taxonomy) but includes `entities/player_entity.h` directly and uses `player_entity:` test titles.
- Section 5 of architecture.md retains "Entity Archetypes" as a concept section heading (not a path reference); body correctly points to `app/entities/`.
- 118 assertions `[archetype]`, 20 test cases / 71 assertions `[model_slice]` — all green, zero warnings.

**File paths touched by this review:**
- `app/entities/player_entity.h`, `app/entities/player_entity.cpp`
- `tests/test_player_archetype.cpp`, `tests/test_obstacle_model_slice.cpp`
- `CMakeLists.txt` (line 102: `ENTITY_SOURCES`)
- `design-docs/architecture.md` (Section 5, line 793+)
- `.squad/decisions/inbox/keyser-archetypes-removal.md`
- `.squad/decisions/inbox/keaton-archetypes-removal.md`
- `.squad/decisions/inbox/mcmanus-archetypes-wording.md`

**Reusable heuristic:** For folder-removal PRs, the three-point check is: (1) directory absent, (2) grep sweep zero matches, (3) tests green. CMake glob cleanup is the most common miss — verify both the glob definition and its use in target sources.

## Learnings — 2026-05-02: Gameplay HUD shape-button review

- Empty-text `GuiButton` still draws a visible rectangular control. If custom circular gameplay buttons are rendered first and `GuiButton` is called after, the rectangle overlays/regresses the circular silhouette and approach-ring readability.
- For "raygui-owned" gameplay controls, keep raygui handling for input/state but make the stock widget visuals transparent or otherwise hidden so custom-drawn affordances remain authoritative.
- A full-slot rectangle hit region is **not** equivalent to legacy 1.4× circular forgiveness. If visual radius is 50 and intended hit radius is 70, rect clipping at ±50 on Y silently removes valid top/bottom taps that previously landed.

## Learnings — 2026-05-02: HUD raw-press gate vs circular forgiveness

- If shape acceptance logic is `if (!raw_pressed) return false`, then effective acceptance is the intersection of raygui widget bounds and the post-filter geometry.
- Therefore, tests that call the circular filter with `raw_pressed=true` are insufficient unless a production path can actually produce `raw_pressed` at those coordinates.
- For 140x100 slots centered at y=1190, taps at y+60/y+70 are outside the widget (`max y=1240`) and cannot be accepted in production even if the circular filter alone returns true.

## Learnings — 2026-05-02: Reachability-safe raygui raw bounds

- If circular gameplay forgiveness is required but raygui still supplies `raw_pressed`, raw input bounds must enclose the intended circular hit radius (not just visual slot bounds) or valid edge taps are unreachable in production.
- A deterministic edge contract (`+70` inside, `+71` outside for visual radius 50 / hit radius 70) is a high-signal regression guard for future HUD geometry edits.
- A small half-extent padding (`+0.5f`) on the enclosing raw square helps avoid precision misses exactly at the intended edge without widening functional gameplay acceptance beyond the tested contract.

## Learnings — 2026-05-02: HUD layout source-of-truth gate

- For gameplay HUD migration, `.rgl` slot geometry and generated layout exports must agree exactly; offsets introduced only in generated/header code are source-of-truth violations.
- `app/ui/generated/*` files are governed as generated, non-hand-edited artifacts in this repo; behavior additions belong in templates/tooling or downstream controller glue, not manual generated-file edits.
- DummyRec-driven custom controls still require auditable alignment across authoring (`content/ui/screens/*.rgl`), generation path, and runtime consumption before approval.

## Learnings — 2026-05-02: HUD raygui migration final gate

- When gameplay shape controls are migrated to raygui ownership, acceptance should require both (a) no runtime `ShapeButtonTag` spawning in play-session setup and (b) preserved shape-input side effects through `ButtonPressEvent` dispatch into existing player input handling.
- Reachability proof must cover the full production chain (`raw raygui bounds` ∩ `circular filter`), not just isolated circle math; deterministic `+70 accepted / +71 rejected` edge tests are a durable regression guard.

## 2026-04-29 — Gameplay shape buttons migration (multi-pass review)

**Status:** COMPREHENSIVE REVIEW CYCLE
**Agent work:** Reviewed 6 revision submissions; identified and enforced blockers; approved final implementation

**Submission review log:**
1. **McManus (R1, 2026-04-29):** REJECTED — Stock rectangular raygui button visuals overlay custom circular silhouettes and approach rings (UX violation). Reassign to Fenster (non-locked).
2. **Fenster (R2, 2026-04-29):** REJECTED — Visual fix valid, but tap-forgiveness regression (140×100 input rectangles cannot reach ±70px band) + geometry drift from `.rgl` slots. Reassign to Keyser (non-locked).
3. **Keyser (R3, 2026-04-29):** REJECTED — Circular filter logic correct, but production reachability blocked: raw raygui bounds (140×100 at y=1140..1240) cannot deliver taps at center.y±70 to the filter. Also identified `.rgl` source drift (60/220/380 vs 90/290/490). Reassign to Baer for reachability + geometry audit.
4. **Baer (R4 / audit, 2026-04-29):** REJECTED — Confirmed geometry source drift as blocker: `gameplay.rgl` slots are x=60/220/380, generated header hard-codes x=90/290/490. Violates source-of-truth requirement. Reassign to Baer for expansion implementation.
5. **Baer (R5 / reachability, 2026-04-29):** APPROVED — Raw raygui bounds expansion to enclose 1.4× forgiveness radius is sound. Reachability contract tests (+70 accept, +71 reject) validated. Circular filter gate preserved. Flagged downstream geometry alignment for final implementer.
6. **Hockney (R6 / final, 2026-04-29):** APPROVED — All 5 acceptance gates pass. Geometry sourced from `.rgl` DummyRec slots, raw bounds expanded, circular filter enforced, semantic ButtonPressEvent path intact, existing side effects preserved. Ready for production.

**Key review decisions:**
- **Acceptance gates enforced (5 total):**
  1. HUD/raygui-owned shape controls; ECS spawning removed
  2. Stock rectangular visuals hidden; circular visuals preserved
  3. Legacy 1.4× circular forgiveness production-reachable
  4. Geometry alignment with `gameplay.rgl`
  5. Pause and existing side effects intact
- **Lockout protocol enforced:** McManus, Fenster, Keyser, Baer (R4) locked after rejection; subsequent revisions reassigned to non-locked agents.
- **Blocker identification:** Visual overlay (R1) → reachability regression (R2) → production bounds blocking (R3) → geometry source drift (R4) → resolution path clarified for final implementation (R5→R6).

**Final validation:**
- Build: zero warnings (clang arm64)
- Test suites: all passing (`[input_pipeline][hud]`, `[gamestate][play_session]`, `[hit_test]`)
- Git diff: clean

**Orchestration logs:** `.squad/orchestration-log/2026-04-29T22-03-09Z-kujan.md`

## 2026-04-29T23:54:05Z — Guard-Clause Refactor Review Gate

Orchestration log written. Review gate on guard-clause refactor completed. APPROVED—no blocking correctness/lifecycle/maintainability regressions identified. Test suite validates high-risk hotspots (dispatcher, RAII, Begin/End pairing).

Decision #169 captured in decisions.md.

---

## 2026-04-30T02:04:27Z — Dead Code Prune — Round 1 Review (Rejected)

**Session:** Multi-agent dead code cleanup.

**Your role:** First-pass review of Keaton/McManus cleanup work.

**Outcome:** ❌ REJECTED. Wording in both code cleanup and doc updates still implied raw input routing emits `ButtonPressEvent`. Core clarity issue: semantic vs raw event routing not properly distinguished. Invoked reviewer lockout; assigned Fenster (non-locked) for independent revision.

---

## 2026-04-30T02:04:27Z — Dead Code Prune — Round 2 Re-Review (Approved)

**Session:** Multi-agent dead code cleanup.

**Your role:** Re-review of Fenster's corrected artifacts.

**Outcome:** ✓ APPROVED. Wording now correctly distinguishes: raw input routing (`InputEvent → GoEvent`), semantic UI events (`ButtonPressEvent` from raygui/controller only). All tests passing (2637 assertions, 795 test cases). `git diff --check` clean. Integration validated.

## Session: Assets Root Removal (2026-04-30)

Reviewed Hockney's asset root removal decision summary. Rejected Verbal's initial `docs/asset-bundle-spec.md` (duplicate sibling `content/` nodes under root). Approved Verbal's corrected diagram (single `content/` node with proper child hierarchy). Validated all changes; no remaining blockers identified.

**Status:** All changes approved; ready for merge.

## Session: Song-Complete Loop Fix (2026-04-30)

**Issue:** Song completes without UI; music repeats.

**Review focus:** Confirm `Music::looping` is the correct raylib seam; verify restart paths preserve `false`; validate test coverage.

**Outcome:** ✓ APPROVED.
- McManus fix correct: `music->stream.looping = false` after `LoadMusicStream` in `app/session/play_session.cpp`.
- Baer regression tests guard latching/phase-transition behavior across two ticks.
- All changes isolated; no collateral surface.
- **Decision logged:** #176 in `.squad/decisions.md` (2026-04-30T07:15:10Z)
