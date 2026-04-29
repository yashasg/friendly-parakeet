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
