# Kujan — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Reviewer
- **Joined:** 2026-04-26T02:12:00.632Z

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

## Learnings

- 2026-05-04T11:08:09Z: Consolidating Mix_* calls into one file is not sufficient; runtime init must fail fast and gameplay-visible timing authority cannot live in hidden static runtime state.
- 2026-05-04T10:56:32Z: When architecture contracts declare "No globals. No virtuals." and "use engine APIs directly," treat compatibility facades (type redefinitions + backend interfaces) as first-class gate failures, not stylistic preferences.

## 2026-05-04T10:56:32Z — Scribe: Team spawn manifest completion

Scribe orchestrated team spawn completion. Your audit findings have been merged to decisions.md.

## 2026-05-04T11:08:09Z — Re-Audit: Audio/Runtime Refactor Gate

**Session:** Kujan + Fenster parallel re-audits on SDL2 migration.

**Your finding:** REJECT for revision. Mixed delta from prior audit:
- ✓ Fixed: SDL_mixer lifecycle/timing no longer duplicated in `app/audio/music_backend.cpp`; Mix_* ownership centralized to `app/runtime/runtime_compat.cpp`.
- ✗ Still open: wrapper abstraction stack (`runtime_types/runtime_compat`, virtual renderer interface), non-ECS runtime static state affecting gameplay time, collision-system duplication.
- ⚠️ New blocker: runtime init not strict; audio init failure ignored in boot path.

**Reassignment guidance:** Route to different implementation agent (not reviewer). Require patch plan removing/narrowing wrappers, relocating gameplay-relevant mutable state into registry context/components, deduplicating collision branches.

**Related:** Fenster audit confirms SDL_mixer consolidation achieved; init contract and shutdown guards remain open.
