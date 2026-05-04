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
- 2026-05-04T00:00:00Z: Audio Mix_* duplication is removed when music_backend only forwards to runtime API, but fail-fast init policy remains weak if init status is discarded and inferred indirectly via readiness checks.

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

## 2026-05-04T11:29:38Z — Independent reviewer audit (criteria gate)

**Scope:** `app/`, `tests/`, issue #374 objective, wrapper/EnTT/SOLID/dedupe criteria.

**Outcome:** REJECT (blocking criteria gaps remain).
- Issue #374 remains open in code and tracker: mutable static audio/runtime state still exists outside ECS context (`RuntimeMusicState`, `RuntimeAudioState`, `MusicTimeOverride`).
- Wrapper surface still broad (`runtime_types/runtime_api` include fan-out plus virtual renderer indirection), conflicting with “no wrappers/use engine APIs” gate and architecture “No virtuals.”
- Collision flow still duplicates shape-gate/combo/split evaluation branches, leaving dedupe/reuse objective incomplete.
- Baseline verification: configure/build succeeded; full tests show one pre-existing failure (`redfoot/#168: existing game_over buttons keep their original positions`), unrelated to this audit.

## Learnings

- 2026-05-04T11:29:38Z: Closing issue #374 requires moving both device lifecycle and music-clock override state into registry context; fixing only init strictness is insufficient.
- 2026-05-04T11:29:38Z: “No wrappers around engine code” must be enforced as an architecture gate when wrapper fan-out reaches core gameplay/rendering callsites.

## 2026-05-04T11:29:38Z — Scribe Session: Decision Merge + Audit Orchestration

**Cross-agent update:** Kujan gate decisions merged into decisions.md (1 inbox file). Orchestration log created.

**Team-relevant outcomes:**
- Gate status: REJECT. Issue #374 remains unresolved blocker.
- Blocking evidence: mutable static audio runtime state, no-wrapper criterion violated (virtual renderer interface still present), architecture/SOLID/EnTT constraints violated, dedupe/reuse incomplete.
- Blueprint completeness check provided (A/B/C/D requirements for resubmission).
- Local baseline: configure/build succeeded; full tests passed (pre-existing unrelated failure noted).

**Team direction:** Require complete implementation blueprint with A/B/C/D details before resubmission. Route to different implementation agent (not reviewer).

**Next phase:** Different agent will revise against blueprint specification and resubmit.

## 2026-05-04 — Scribe: Audio audit outcomes logged
- Audit gate gate REJECT: Issue #374 remains OPEN
- Audio device runtime state must move into ECS context (next priority)
- Orchestration logs written; decisions.md merged
