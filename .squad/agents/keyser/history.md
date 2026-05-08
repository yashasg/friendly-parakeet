# Keyser — History (Summarized)

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Lead Architect
- **Joined:** 2026-04-26T02:07:46.542Z

## Executive Summary

Conducted 17 audit rounds focusing on SOLID compliance, module health, system wiring verification, and architectural refactoring guidance. Key achievements: Identified and escalated production bugs (lane_push_response_system wiring, order regressions), audited and reconciled Position migration (#349), established forensic procedures for test count validation, and maintained module health snapshot across complex multi-round iterations.

**Current Module Status:** 11 🟢 (green) / 1 🟡 (pending) — motion_system awaiting complete Position bridge deletion (follow-up to R17)

## Key Learnings

1. **Audit findings labeled 'redundant' must trace ALL invocation paths** — Event-dispatcher callbacks invoke functions outside static runners; guards protecting dispatcher callbacks aren't redundant just because the runner gates other paths.

2. **Parallel agent edits are swept up by `git add -A`** — Scribe must use explicit `git add` paths; commit-message attribution can diverge from actual diff location.

3. **Test assertions claiming pre-fix failure are vibes claims** — Demand fail-then-fix verification (stash production, run tests, unstash) before trusting a test comment's claim about pre-fix behavior.

4. **Synthetic benchmarks must be re-validated after archetype refactors** — Splitting a system can drop fixture matching to zero, leaving the bench measuring nothing.

5. **When implementation hasn't landed, audit only stable commits** — Don't speculate on in-flight work; pre-document baselines for post-implementation comparison.

6. **Self-wiring tests mask production bugs** — Tests calling systems directly short-circuit integration verification. Demand at least one test that calls the production tick path.

7. **Methodology inconsistency generates false alarms** — Canonical commands + verbatim output enforce discipline. Filter-flag asymmetry (r10: no-filter 798 total; r11: ~[bench] 783 non-bench) can look like regression.

8. **When flagging commit absence, verify gitignore status first** — Inbox files are gitignored by design; a missing file in git doesn't indicate process failure.

## Recent Work

### R17: End-to-End Position Deletion Audit

**Date:** 2026-05-03  
**Scope:** r16 Position deletion (7ae9659) + r17 WIP latent fix

**Findings:**
- F1: decisions.md incorrectly credited r16; fix was r17. Corrected via surgical edit.
- F2: r16 inbox absence initially flagged 🔴 — verified as gitignored by design (not a process miss).
- Position deletion successful: all critical sites (collision, scoring, camera, scroll) migrated correctly.
- Test count drop (786→784, 2256→2234): all deletions correctly attributed.
- Motion system latent fixed in r17 via Option A (exclude<ObstacleScrollZ>).

**Verdict:** All modules 🟢. Loop at natural diminishing returns.

## Historical Round Snapshots

**R2–R7:** Ralph performance + SOLID audit loop; identified SRP issues in scroll_system, BarObstacleTag refactor, LanePush critical production bug (system declared but not wired).

**R8–R9:** Phase-Guard Design B scope; 11 systems consolidated into central runner; Keaton-r9 refactor landed in `tick_playing_systems`.

**R10–R12:** Order regression discovery (popup_feedback + energy_system moved pre-despawn, breaking design intent); test count forensic (methodology drift r10→r11, no actual deletion); collision_system SRP + scoring_system kind-independence audit.

**R13–R15:** Chain-bonus retraction (independent grep confirmed), SRP audit, fixed_tick_runner demotion reversal (commutativity proof), motion_system latent regression (Velocity → MotionVelocity migration debt).

**R16–R17:** Position bridge audit, Position deletion completion, motion_system latent fix landed.

## Status

**Module health snapshot (post-R17):** 11 🟢 / 1 🟡 (motion_system; pending bridge audit for r18)

**Next:** Audit motion_system Position bridge for completeness; recommend surfacing R17 audit to user for decision on architecture direction or further cleanup.
