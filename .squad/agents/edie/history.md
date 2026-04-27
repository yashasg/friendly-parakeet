# Edie — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Product Manager
- **Joined:** 2026-04-26T02:12:00.632Z

## Learnings

<!-- Append learnings below -->

### 2026-04-26 — Role fit review for SHAPESHIFTER
- Project is a C++20/raylib/EnTT rhythm bullet hell, song-driven beatmaps via Python pipeline (rhythm_pipeline.py + level_designer.py), 386 Catch2 tests, multi-platform CI (Linux/macOS/Windows + WASM), Catch2.
- 13 active specialists; coverage spans architecture, gameplay, tools, platform, QA, perf, CI/release, product, UX, game design, level design, test eng, review.
- Scribe (silent) and Ralph (monitor) are infra roles, not feature contributors.
- Potential overlap: Verbal (QA) vs Baer (Test Eng); McManus (gameplay) vs Saul (game design) vs Rabin (level design) — distinct in charters but watch for duplicated work on rhythm/obstacle tuning.

### 2026-04-26 — Product diagnostics pass on design-docs/, docs/, content/
- **Major scope contradiction**: `game.md` (v1.1) frames the product as an **endless runner** with **Burnout** as the core mechanic and 4 shapes (Circle/Square/Triangle/Hexagon). `rhythm-design.md` and `rhythm-spec.md` (both v1.1) explicitly state "This is NOT a bullet hell game with optional rhythm mode. This IS a rhythm game" with **proximity ring replacing burnout**, **MISS = instant game over (HP removed)**, and 3 active shapes + Hexagon as rest. README.md aligns with the rhythm framing (Perfect/Good/Ok/Bad timing grades). Still no decision recorded in `.squad/decisions.md`.
- **Death-model trilemma**: three live specs disagree about player failure: `game.md` (DEAD zone in burnout = game over), `rhythm-spec.md` (MISS = instant game over, no HP), `energy-bar.md` v1.0 (replaces instant-death-on-miss with energy drain). Implementation cannot satisfy all three.
- **Platform mismatch**: `game.md` says "Mobile — portrait mode only", README advertises desktop CI (Linux/macOS/Windows) + WebAssembly playable in browser. No iOS/Android target, project, or store metadata exists. Touch controls spec'd but keyboard mapping is the shipping path.
- **App-Store readiness gaps**: no `LICENSE`, no `CREDITS`/`NOTICE`, no audio attribution (3 .flac files in `content/audio/` with no artist/license metadata in beatmap JSON), no privacy policy, no rating/age guidance, no app icons/screenshots/store-listing assets, no version field, no CHANGELOG, no TestFlight/Play-Console plan.
- **Persistence undefined**: high score and `ftue_run_count` referenced as persistent but no spec for storage backend per platform (localStorage on WASM vs file on native vs UserDefaults on iOS).
- **Obstacle taxonomy contradiction**: `game.md` deprecates LaneBlock in favor of LanePushLeft/Right, but `feature-specs.md` Spec 3 still defines `LaneBlock = 1` in `ObstacleKind` enum and references it.
- **Burnout vs rhythm pipeline conflict**: `feature-specs.md` defines the entire spawning system as **time-based difficulty ramp 0–180s**, but `rhythm-design.md` says obstacles are **driven by beatmap/audio analysis**. Speed multiplier table in `game.md` and `feature-specs.md` is incompatible with fixed-BPM song-driven scrolling in `rhythm-spec.md`.
- **Accessibility**: no colorblind alternative for the 3 shape colors (critical because shape gates are the only obstacle type at low difficulty), no audio-off/visual-cue alternative (critical for a rhythm game), no haptics-off path on web.
- **Pause/resume under audio**: pause behaviour is spec'd to freeze `dt` (burnout system), but `rhythm-spec.md` derives obstacle timing from `song_time`. Whether `PauseAudioStream`/`ResumeAudioStream` is called and how rejoin handles audio buffer reset is not specified.
- **FTUE specified, P3-prioritised**: `game-flow.md` defines 5 tutorial runs, but the same doc puts FTUE in **P3 / "do it when everything else works"**. New-player onboarding is currently a release blocker that's been deprioritised.
- **Content scope**: only 3 songs shipped (`1_stomper`, `2_drama`, `3_mental_corruption`). No content roadmap, no song-pack delivery plan, no level-select acceptance criteria beyond "list of available songs".
- **Test-player coverage**: pro/good/bad AI personas exist, but no human UAT plan, no playtest checklist, no closed-beta gate before public release.

### 2026-04-28 — Fresh release-readiness diagnostics pass
Filed 17 NEW issues focused on iOS-specific TestFlight/App Store readiness gaps not previously captured by #45-162. Skipped 0 duplicates. Key insights:

- **iOS lifecycle gap is the largest single blind spot.** The repo treats the rhythm pipeline as if `song_time` is always running. iOS audio-session interruptions (#180), app backgrounding (#182), and ProMotion variable refresh (#204) each break that assumption silently. Existing #74 covers manual pause only, not OS-driven events.
- **Submission-mechanics gaps are zero-dollar but blocking.** No bundle ID/signing plan (#184), no version-number scheme (#183), no privacy-label answers (#189), no age-rating answers (#190), no ATT stance (#194). These are paperwork/policy items that gate the very first TF upload and must be locked before code work matters.
- **Telemetry/crash-reporting (#185) is the multiplier on TF value.** Without it, TestFlight produces only qualitative feedback; with it, every TF cohort yields actionable signal. Decision intersects with privacy labels (#189) and ATT (#194) — these three should be resolved together.
- **Three product decisions still open:** unlock model (#195), Apple services scope (Game Center/iCloud) (#192), localization (#191). All are AppStore-milestone "decide explicitly even if the answer is 'no'" tickets.
- **Asset and device matrix:** FLAC shipping (#188) and 120Hz timing fairness (#204) both have hidden cross-device fairness/perf implications for a rhythm game where every ms counts.
- **Process gap:** No documented TestFlight beta plan (#201) — without it, the test-flight milestone graduates no signal into App Store readiness.

---

**Wave Summary (2026-04-26T23:40:25Z):**
- Full diagnostics pass completed; 76 issues filed across 7 domains.
- Edie active: #185, #188, #201 in current wave; all three tied to AppStore planning gaps noted above.
- Orchestration log: `.squad/orchestration-log/2026-04-26T23:40:25Z-wave-summary.md`.

### 2026-04-28 — TestFlight readiness decisions (#185, #188, #201)
Created `docs/testflight-readiness.md` v1.0 as the canonical TF-program doc. Posted scoped decision comments on all three issues; left them open pending engineering follow-ups. Key decisions:

- **#185 telemetry/crash:** Apple-first, **zero third-party SDK** for TF (MetricKit + TestFlight crash logs only). Local `session_log` captures the funnel event set (`app_launch`, FTUE start/complete/abandon, song start/complete/fail, `pause_used`, `audio_interrupt`, `app_background`) with **no PII, no IDFA/IDFV, no free-form text**. ATT prompt **not required**. Privacy labels: Data Used to Track = None, Data Linked = None, Data Not Linked = Diagnostics. Reversible — documented re-evaluation triggers (signal precision, MetricKit lag, real-time funnel need).
- **#188 audio:** IPA budget **≤ 100 MB**, audio bundle **≤ 35 MB**. Ship **AAC-LC `.m4a` 192 kbps CBR / 44.1 kHz stereo** (256 kbps held as fallback). Current FLAC bundle is ~88 MB, blows budget on its own. Beat-drift policy: **regenerate beatmaps against shipping format**, do not assert FLAC invariance; pin ffmpeg version. Validation: 6-row matrix (perceptual A/B, aubio onset Δ p95 ≤ 10 ms, beatmap regen diff within 1 frame @ 60 Hz, in-game timing ±5 ms, file size, cold-start ≤ 2.0 s). This task does **not** transcode — separate impl ticket for Fenster+Hockney.
- **#201 beta program:** 3-phase rollout (closed alpha 5–10 → closed beta 25–50 → open beta ≤ 200). Welcome + what-to-test checklist drafted in-doc, includes audio-interrupt and lifecycle scenarios. Feedback intake = **TestFlight in-app + invite email reply only** (single queue, no Discord/forms in v1). SLA: Sev-1 24 h response / 48 h fix-or-defer; weekly subjective review. Go/no-go = crash-free session ≥ 99 %, FTUE completion ≥ 70 %, 0 Sev-1, ≤ 3 Sev-2, audio-interrupt 100 %, cold-start ≤ 2.0 s, privacy/signing locked. Sign-off Edie+Kobayashi+Verbal, ≥ 7-day soak.

**Cross-cutting flags raised in §4 of the doc:** #185 depends on #189/#194/#180/#182; #201 depends on #183/#184 before first build can ship. The three beta-readiness gates (telemetry, audio, program) are now decoupled enough to land in parallel impl tickets.

**Process note:** chose to put this in `docs/` (not a top-level `*_SOLUTION.md`) because it's an evergreen program doc, not an issue-completion artefact.

Comments:
- #185 → https://github.com/yashasg/friendly-parakeet/issues/185#issuecomment-4323324681
- #188 → https://github.com/yashasg/friendly-parakeet/issues/188#issuecomment-4323324708
- #201 → https://github.com/yashasg/friendly-parakeet/issues/201#issuecomment-4323324733

**Final Wave (2026-04-26):**
- #185/#188/#201: Created `docs/testflight-readiness.md` (TestFlight prerequisites, app icons, signing, capabilities, device matrix)
- Documented 5 user-provided blockers (Team ID, bundle ID confirmation, app icons, program type, build number bump)
- Comments posted to all three issues
