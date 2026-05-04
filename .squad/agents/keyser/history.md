# Keyser — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Lead Architect & SOLID Reviewer
- **Joined:** 2026-04-26T02:07:46.542Z

## Learnings (Active)

### Audit Findings & Pattern Codification

**Audit Findings labeled "redundant" must trace ALL invocation paths:** Event-dispatcher callbacks invoke functions outside static system runners. Guards protecting dispatcher paths aren't redundant just because the runner gates other paths. Before declaring a guard redundant: (a) prove all calling sites are gated (including indirect dispatcher paths), or (b) confirm failing test when guard is removed.

**Tag-replacement pattern is now ratified:** Used successfully 3 times (NonScorableTag, BarObstacleTag, LanePushDelta). Recipe: replace inline enum-branch with tag check, update tests to synthetic kind proof + else-branch validation. SOLID clean per Ralph audits R5–R9.

**SOLID audit checklist (Ralph audits):**
- SRP: no single responsibility violations
- Phase-gate design: single early-return, guards dropped/consolidated confirmed
- DIP: no direct interface pollution

**Architecture decision patterns:**
- Framework objections must distinguish (a) limitation, (b) workflow blocker, (c) scope creep. Raylib SDL2+LVGL objection was (b) + (c) masquerading as (a).
- Raylib abstraction shields codebase from SDL2/3 churn; no direct SDL exposure. Low migration urgency unless ecosystem mandates.

### Phase-Guard Design B (approved for implementation)

11 systems affected (obstacle_despawn_system has no guard). Recommended: hand-written `tick_playing_systems(reg, dt)` inline `if` block. Net diff: +3 lines (~28 touched); removes duplication, not just renames it.

**Critical fix in flight (R11):** ORDER REGRESSION found in R9 — `popup_feedback_system` and `energy_system` moved before `obstacle_despawn_system`, breaking "score-feedback chain contiguous" design intent. Prefer revert; if documented intentional, update comment at `game_loop.cpp:188`.

## 2026-05-04 — Ralph Round 5: collision_system Audit

ECS collision resolution matrix: significant duplication/SRP erosion via branched duplicate loops for graded/ungraded shape obstacles. Recommend deduplicating via reusable evaluator helpers.

## 2026-05-04 — Ralph Round 6: NonScorableTag Verification + Tag-vs-Kind Audit

Verified tag-replacement pattern: NonScorableTag behaves identically to prior `ObstacleKind::NonScorableObstacle` branch. Kind-independence + else-branch tested; scoring_system SOLID clean.

## 2026-05-04 — Ralph Round 9: Wirefix Audit + Self-Wiring Meta-Scan

All 11 phase-guarded systems wired correctly. No dead code. Phase-gate single early-return confirmed; 11 guards confirmed dropped.

## 2026-05-03T23:47 — Raylib vs SDL2+LVGL Objection (Framework Architecture Decision)

**Verdict:** KEEP raylib.

**Rationale:**
- Real blocker is iOS credentials (issue #184), not framework limitation.
- iOS support exists and is functional; deployment is credentials-blocked.
- Cost of SDL2+LVGL swap (3–4 weeks) exceeds benefit (Metal perf N/A, LVGL widgets N/A) for v1.
- Risk of regression on 5 active platforms during migration is unacceptable for non-critical feature.
- Raylib dependency audit: 60 includes, 315 API callsites. Type + lifecycle coupling dominate difficulty (classes B/C).

**Recommendation:** Clarify owner intent: Is iOS a hard v1 blocker? If yes, provide credentials. If no, defer iOS to v2 and ship Desktop + WASM as v1. Do NOT use iOS as pretext to re-engine I/O stack.

## 2026-05-04 | SDL2 vs SDL3 Comparison Analysis

**Key Findings:**
- Raylib abstraction shields codebase from SDL2/3 churn; no direct exposure.
- SDL3 (v3.1+) is production-ready but ecosystem still maturing.
- SDL2 stable, well-supported; raylib will maintain SDL2 support indefinitely.
- SHAPESHIFTER bottleneck is game logic (audio sync, obstacle spawning), not rendering. GPU-first SDL3 does not directly improve our case.

**Recommendation:** Maintain raylib now. Re-evaluate Q4 2026 if raylib stalls or SDL3 ecosystem reaches production maturity.

## 2026-05-04T00:18:28Z — Raylib-to-SDL2 Migration Plan (Planning Phase)

Created comprehensive, execution-ready migration plan (22K+ characters).

**Phasing Strategy (7 phases):**
1. Abstraction layer (renderer, input, window interfaces)
2. SDL2 window & OpenGL context (native platforms)
3. Core rendering (floor, obstacles, particles)
4. Input system (keyboard, mouse, touch, gestures)
5. Audio and frame timing
6. Emscripten (WASM) and cross-platform hardening
7. Cleanup and raylib removal

**Total Effort:** ~124–180 hours (6–8 weeks, 1 person part-time)

## 2026-05-04T10:56:32Z — Scribe: Team spawn manifest completion

Scribe orchestrated team spawn completion. Your audit findings have been merged to decisions.md.

**See:** `.squad/agents/keyser/history-archive.md` for Ralph Rounds 3–4, phase-guard design details, and detailed SOLID analyses (2026-05-03 through 2026-04-29).

## 2026-05-04T11:29:38Z — Fresh full audit (post-fix) on wrappers/SOLID/ECS

- Issue #374 remains open: mutable audio runtime state is still held in static globals (`RuntimeMusicState`, `RuntimeAudioState`, `MusicTimeOverride`) rather than ECS context.
- Wrapper criteria still fail at architecture level: `runtime_types/runtime_compat` facade + virtual renderer indirection remain broad and gameplay-adjacent.
- Major dedupe/SOLID gap still in `collision_system.cpp` (ShapeGate/ComboGate/SplitPath branch matrix duplicated across graded/ungraded paths).
- EnTT eager-init policy drift persists in hot path: `collision_system` still does `ctx().find<SongState>()` + fallback `emplace` despite startup singleton init contract.

## 2026-05-04T11:29:38Z — Scribe Session: Decision Merge + Audit Orchestration

**Cross-agent update:** Keyser audit decisions merged into decisions.md (3 inbox files). Orchestration log created. 

**Team-relevant outcomes:**
- Full architecture audit completed; verdict REJECT for architecture completion.
- Issue #374 remains OPEN blocker; ECS authority breach (audio runtime state in statics) + wrapper criterion violations + collision dedupe gap.
- Provided complete implementation blueprint for #374 safe completion path (8-point detailed specification).
- Baseline: build + tests passed.

**Next phase:** Route revision to different implementation agent (not reviewer). Require patch plan addressing blueprint specifics.

## 2026-05-04 — Fresh full audit after latest audio-state migration

- Issue #374 is **not fully satisfied**: music timing/override state is now ECS-owned in `MusicContext` (`app/audio/music_context.h`, `app/audio/music_backend.cpp`), but mutable audio device lifecycle state remains process-global in `app/runtime/runtime_compat.cpp` (`RuntimeAudioState` + `runtime_audio_state()`).
- Wrapper criterion still fails: broad engine-compat and virtual renderer indirection remain active in gameplay paths (`app/runtime/runtime_types.h`, `app/runtime/runtime_api.h`, `app/rendering/renderer_backend.h`, `app/rendering/renderer_backend_sdl2.cpp`).
- SOLID/dedupe hotspot remains in `app/systems/collision_system.cpp` (graded/ungraded ShapeGate/ComboGate/SplitPath branch duplication; lazy `SongState` `find()+emplace()` fallback in hot path).
- Fresh validation baseline: build succeeds; tests show one pre-existing unrelated failure (`build/ctest-latest.log`: test 668, `redfoot/#168: existing game_over buttons keep their original positions`).

## 2026-05-04 — Scribe: Audio audit outcomes logged
- Audit gate gate REJECT: Issue #374 remains OPEN
- Audio device runtime state must move into ECS context (next priority)
- Orchestration logs written; decisions.md merged

## 2026-05-04T11:55:54Z — Kujan post-audio criteria audit complete

Kujan completed full post-audio architecture audit against criteria (no-wrappers, SOLID, dedupe, EnTT principles).

**Audit verdict: PASS on Issues #373/374/375 (confirmed CLOSED); 2 new blockers identified for next cycle.**

**Top findings:**
1. Blocker 1 (P0): Virtual renderer wrapper violates no-virtuals + no-wrappers-around-engine criteria
   - Evidence: `class Renderer` ABC with single impl `Sdl2Renderer` (no polymorphism benefit)
   - Patch target: Remove ABC; replace with free functions in `platform::graphics` namespace
2. Blocker 2 (P1): Collision system violates EnTT init contract + duplicates gate logic
   - Evidence: `collision_system.cpp` lazy `ctx().find<SongState>()` + fallback `emplace`; can_grade_shape branches duplicate ShapeGate/ComboGate/SplitPath loops
   - Patch target: Move `SongState` emplace to `game_loop_init`; unify gate branches into single loop

**Routing:** Blocker 1 to rendering specialist (broader callsite surface); Blocker 2 to any implementation agent (self-contained). Both can execute in single pass.

**Status:** Findings merged to decisions.md; orchestration logged. Ready for implementation handoff.
