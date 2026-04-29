# Scribe — History

## Core Context

- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Session Logger
- **Joined:** 2026-04-26T02:07:46.548Z

## Learnings

- **Portable rGuiLayout Spec Consolidation (2026-04-29T04:35:58Z):** Merged decision #190 from inbox to decisions.md. Keyser's spec clarified portable C as primary architectural contract; Fenster cleaned adapter terminology; Coordinator approved revised spec targeting screen_controllers, not adapters. Decision now public in team records. Orchestration and session logs created; no commits per charter.
- **Issue #142 (2026-04-26T08:36:55Z):** Medium balance fix completed. Tight integration between Ralph (initial impl), Coordinator (finalization + content regen), and Kujan (review gate). All orchestration logs created; no git commits made per charter.
- **Decision inbox:** Remains empty for this session; no new team decisions required.
- **Issue #54 (2026-04-26T09:08:09Z):** MSVC warning policy fix completed. Ralph wired `/W4` and `/WX`; Coordinator tightened CMake and resolved utility-source link validation; Kujan approved. Session and orchestration logs created; GitHub comment posted.
- **Issue #56 (2026-04-26T09:12:58Z):** vcpkg overlay cache-key fix completed. Ralph added `vcpkg-overlay/**` to relevant cache hashes; Coordinator validated all workflow keys; Kujan approved. Session and orchestration logs created; GitHub comment posted.
- **Issue #59 (2026-04-26T09:18:54Z):** WASM `NO_EXIT_RUNTIME` fix completed. Ralph added target-level Emscripten linker flag; Coordinator tightened docs and validated native/tests plus CMake sanity; Kujan approved. Session and orchestration logs created; GitHub comment posted.
- **Issue #62 (2026-04-26T09:37:50Z):** WASM CI test execution fix completed. Ralph added the test path; Coordinator corrected it to CTest + Node and validated real WASM tests; Kujan approved. Session and orchestration logs created; GitHub comment posted.
- **Issue #64 (2026-04-26T09:48:12Z):** Shipped beatmap CI validation fix completed. Ralph added shipped beatmap coverage and removed content ignores; Coordinator tightened exact-difficulty assertions and validated malformed failure; Kujan approved. Session and orchestration logs created; GitHub comment posted.
- **Issue #66 (2026-04-26T09:58:41Z):** Product scope contradiction resolved. Docs now consistently define TestFlight as Song-Driven Rhythm Bullet Hell with 3 songs and Level Select; freeplay/random spawning is post-release/dev fallback only. Coordinator validated, Kujan approved, session/orchestration logs created, and GitHub comment posted.
- **Issue #67 (2026-04-26T10:15:16Z):** Death model contradiction resolved. Energy Bar is now the sole death authority in docs/tests/runtime; Burnout Dead-zone is x5 scoring feedback only; float-residue depletion is clamped with `ENERGY_DEPLETED_EPSILON`. Coordinator validated focused/full native tests and executable build; Kujan approved; session/orchestration logs created; GitHub comment posted.
- **Issue #68 (2026-04-26T10:24:40Z):** Platform scope locked. TestFlight and App Store v1 are iOS-only customer releases; macOS/Linux/Windows/WASM are CI/dev validation surfaces. Coordinator cleaned platform wording and fixed Kujan's numbered-list finding; Kujan approved on re-review; session/orchestration logs created; GitHub comment posted.
- **Issue #71 (2026-04-26T10:38:40Z):** Persistence backend policy specified. Docs now define iOS app-sandbox JSON persistence, desktop CI/dev filesystem paths, WASM CI/dev current-directory VFS, current high-score/settings data, and FTUE run count as a required but unimplemented v1 field. Coordinator removed iCloud/IDBFS overclaims, validated persistence/full native tests, Kujan approved on re-review, session/orchestration logs created, and GitHub comment posted.
- **Issue #72 (2026-04-26T10:51:51Z):** LaneBlock/LanePush taxonomy reconciled. Shipping lane obstacles are now `lane_push_left`/`lane_push_right`; `lane_block`/`LaneBlock` is legacy non-shipping compatibility only. Shipping validation/tooling reject lane_block, docs and agents were updated, focused/full native tests plus Python beatmap validators passed, Kujan approved, session/orchestration logs created, and GitHub comment posted.
- **Issue #73 (2026-04-26T11:04:15Z):** Difficulty ownership reconciled. Song mode now documents EASY/MEDIUM/HARD as authored beatmap variants, while freeplay/random spawning owns the elapsed-time `DifficultyConfig` ramp. Runtime skips difficulty and random spawning while songs play; stale speed-ramp/speed-bar docs were cleaned, focused/full native tests passed, Kujan approved, session/orchestration logs created, and GitHub comment posted.
- **Issue #74 (2026-04-26T11:23:19Z):** Pause/resume audio sync specified. Music pauses, `song_time`/beats/gameplay freeze, resume is explicit with no countdown/grace, stale input is cleared, focus loss enters Paused without auto-resume, and iOS interruption QA is documented. Coordinator added pause guards for cleanup/lifetime/particles and fixed cleanup energy epsilon clamping; Kujan approved on re-review; session/orchestration logs created, and GitHub comment posted.
- **Issue #76 (2026-04-26T21:50:49Z):** Minimal FTUE/tutorial implemented. Fresh installs route Title START to Tutorial, Tutorial START persists `ftue_run_count=1` and opens Level Select, completed users skip Tutorial, and docs scope the full 5-run FTUE to future expansion. Coordinator validated JSON/assets/focused/full native tests; Kujan found no significant issues; session/orchestration logs created, and GitHub comment posted.
- **Issue #82 (2026-04-26T21:58:33Z):** MISS semantics reconciled. Rhythm/Energy Bar docs now agree that MISS drains energy, never directly requests GameOver, and `energy_system()` owns death on zero/epsilon depletion. Cleanup/offscreen misses are documented and tested; focused/full native tests passed; Kujan approved with one non-blocking Bad-timing test note; session/orchestration logs created, and GitHub comment posted.
- **Issue #92 (2026-04-26T22:18:46Z):** Same-beat beatmap validation fixed. Validator now allows non-decreasing beats with compatible simultaneous action slots, rejects duplicates/conflicts including ComboGate shape stacking, docs match the policy, scheduler same-beat spawning is covered, focused/full native tests passed, Kujan approved, session/orchestration logs created, and GitHub comment posted.
- **Issue #99 (2026-04-26T22:27:16Z):** Energy Bar tuning made TestFlight-forgiving. Target is 8 misses survive / 9th depletes, Perfect cancels Miss, Good cancels Bad, constants/docs/tests were updated, focused/full native tests passed, Kujan approved after stale table correction, session/orchestration logs created, and GitHub comment posted.
- **Issue #111 (2026-04-26T22:30:03Z):** Session log MISS regression coverage added. `MissTag` is the log authority, fatal/non-fatal session-log fixtures assert `result=MISS` and not `CLEAR`, focused/full native tests passed, Kujan approved, session/orchestration logs created, and GitHub follow-up posted.
- **Issue #114 (2026-04-26T22:34:20Z):** SongResults total note coverage added. All shipped levels/difficulties now enter play and assert `SongResults.total_notes` equals loaded beat count and resolves for Song Complete UI, focused/full native tests passed, Kujan approved, session/orchestration logs created, and GitHub follow-up posted.

---

**Wave Summary (2026-04-26T23:40:25Z):**
- Full TestFlight/AppStore diagnostics completed: 76 unique issues filed (163–238, zero duplicates with #44–#162).
- Five decisions merged to `.squad/decisions.md` (#125, #134, #135, #46, #167; all APPROVED/IMPLEMENTED).
- Six inbox files processed, deduplicated, and deleted.
- Orchestration log: `.squad/orchestration-log/2026-04-26T23:40:25Z-wave-summary.md`.
- Session log: `.squad/log/2026-04-26T23:40:25Z-testflight-wave.md`.
- Three TestFlight blockers identified: #222 (aubio), #172 (WASM CI), #236 (haptics).
- Agent histories updated (Baer, Edie, Fenster, Hockney, Kobayashi, Kujan).

---

**Cleanup Wave (2026-04-27T02:51:05Z):**
- **Request:** Ralph (GitHub issue cleanup), Keyser (ECS architecture cleanup), Keaton (C++ performance/warnings), Baer (test suite refactoring), Kujan (code review gate), Scribe (orchestration logging).
- **Model routing:** claude-sonnet-4.6 (default: Keyser, Keaton, Baer, Kujan); claude-haiku-4.5 (Ralph, Scribe).
- **Scope:** GitHub backlog triage, ECS component/system refactoring, C++ optimization, test coverage improvements. No product code or issue modifications. Zero warnings maintained.
- **Orchestration log:** `.squad/orchestration-log/2026-04-27T02:51:05Z-cleanup-wave.md`.
- **Status:** LAUNCHED (parallel cleanup agents + Kujan review gate).

---

- **Screen Controller Migration & Validation (2026-04-29T05:03:05Z):** Keaton implemented adapter removal and screen_controllers migration (deleted `app/ui/adapters/`, created `app/ui/screen_controllers/` with template-based abstraction reducing 377 lines of boilerplate to ~50 lines template + 8 declarations). Hockney validated zero-warning builds, 2635 assertions, 901 tests passing. Fenster fixed stale app-code comments and polished spec wording. Kujan re-reviewed and approved after spec clarification. Orchestration log: `.squad/orchestration-log/2026-04-29T05-03-05Z-screen-controller-migration-batch.md`. Session log: `.squad/log/2026-04-29T05-03-05Z-screen-controller-migration.md`. No new decision inbox; prior inbox now empty. No git commits made per charter.
