# Saul — History (SUMMARIZED)

**Last Updated:** 2026-05-10  
**Current Focus:** Design document lifecycle and governance  
**Status:** Proposal for archive/ convention approved; prototype.md migration in progress

## Quick Reference

- **Decision:** Superseded docs move to design-docs/archive/; live docs stay in design-docs/
- **Archive Header Must Include:** ARCHIVED banner, reason (issue link), list of replacement docs
- **Scope:** Only prototype.md moved in this pass; future archivals follow same pattern
- **Deferred:** Archive README.md index when ≥ 3 entries

---

# Saul — History

## Core Context

- **Owner:** yashasg
- **Project:** A C++20 raylib/EnTT rhythm bullet hell game with song-driven obstacles and shape matching synced to music.
- **Role:** Game Designer
- **Joined:** 2026-04-26T02:12:00.632Z

## Learnings

<!-- Append learnings below -->

## 2026-04-26 · Game-Design Diagnostic Pass
- The game has two parallel survival/scoring models that contradict each other: the legacy **Burnout** model (DEAD zone = game over, multiplier x1→x5 risk/reward) and the newer **Energy bar** model (miss → drain, 0 energy = game over). Code only honors the energy bar; the burnout DEAD zone never kills and instead silently grants the x5 multiplier. The GDD (`design-docs/game.md`) and `feature-specs.md` still describe the burnout-kill model.
- Burnout zones are distance-based with `ZONE_DANGER_MIN = 140 px`, while collisions only resolve within `COLLISION_MARGIN = 40 px` of the player. Therefore at the moment scoring runs, the meter is *always* in `Dead`, so every successful clear receives x5. The risk/reward gradient (Safe/Risky/Danger) the GDD sells is non-functional in rhythm mode.
- Beatmap content uses only `shape_gate` and `lane_push_*`. `low_bar`, `high_bar`, `combo_gate`, `split_path`, and `lane_block` are wired in code but have **zero** content. Half the GDD's obstacle table is vestigial.
- 2_drama medium has 69/105 shape gates as `square` in lane 1 — same gate, same lane, two-thirds of the song. The shape system isn't really exercised by the content.
- High score (`ScoreState.high_score`) is updated in memory but never written to disk; it resets every launch. Big regression for a "one more try" loop and a non-starter for TestFlight.
- FTUE (`design-docs/game-flow.md` §3, "ftue_run_count", 5 tutorial runs) is fully spec'd but **not implemented**. No code references "tutorial" or "ftue".
- Energy tuning: a single `Bad` (-0.10) wipes 5 `Ok` hits (+0.02). Combined with `Miss` (-0.20), early players will collapse fast. Designed scenarios assume a forgiving recovery curve that the constants don't actually deliver.
- Chain timer resets after 2.0s of no score. Several beatmap sections (intros, breaks, gap≥4 phrases) exceed this naturally → chain bonus is unreachable on slower songs.
- `ScoreState.distance_traveled` accumulates from `DifficultyConfig.scroll_speed`, but in rhythm mode `difficulty_system` early-returns and never re-syncs to BPM-derived speed, so distance shown on Game Over is wrong.


## 2026-04-26 · #135 Difficulty Ramp Target
- Audited shipped beatmaps: easy is 100% shape_gate (no kind variety, but shape/lane variety are usable axes); medium LanePush share is 14% (stomper), **28% (drama, over budget)**, 12% (mental). First LanePush lands at beat 57 / 92 / 54 — i.e., ~21–35s in. No telegraph or shape-only intro is currently enforced.
- Easy must NOT acquire lane_push or bars (would break #125 contract). Variety has to come from inside the shape_gate space: lane distribution, gap variety, section-aware density, and shape rotation that respects `min_shape_change_gap=3` (#134).
- Medium needs LanePush as a *taught mechanic*, not a sudden 30s-in cliff: shape-only intro until later of (30s, first chorus), 4-beat telegraph + recovery around first 3 LanePushes, push-toward-center on wall lanes (lane 0 → push_right only), share cap 20%, min gap=3 beats between LanePushes.
- Wrote `.squad/decisions/inbox/saul-difficulty-ramp-135.md` with concrete constants and two new `level_designer.py` passes (`enforce_lane_push_ramp`, `enforce_easy_variety`) for Rabin.

## 2026-04-27 · #135 Team Completion
- **Status:** ✅ APPROVED
- Initial implementation (Rabin): ❌ REJECTED by Kujan due to easy lane_push contamination (1.6–4.1%) violating the design target.
- Revision (McManus + Verbal): ✅ APPROVED. Set `LANEPUSH_RAMP["easy"] = None`, disabled injection entirely. All 9 beatmaps regenerated: easy 100% shape_gate, 3 shapes, dominant ≤60%. Medium lane_push 9.3–19.5%, max consecutive ≤3, start_progress 0.30 respected. Hard bars intact (stomper 1/3, drama 2/2, mental 7/7).
- New guard established: every difficulty contract test must include (1) kind-exclusion assertions and (2) distribution/variety assertions. Verbal added `[shape_gate_only]` C++ test + Python `check_easy_shape_gate_only()`.
- All 2366 C++ assertions pass (730 test cases). Decision merged to `.squad/decisions.md` as "#135 — Difficulty Ramp: Easy Variety + Medium LanePush Teaching".

## 2026-04-28 · Fresh Game-Design Diagnostic Pass
- Scanned the existing #44–#162 squad-saul backlog before filing. Avoided dups against #67, #79, #96, #99, #100, #101, #102, #103, #104, #105, #114, #120, #135, #142, #161.
- Filed five fresh issues from a scoring/balance/progression lens; all rooted in concrete code citations:
  - **#167** (test-flight, bug): `COLLISION_MARGIN=40 < ZONE_DANGER_MIN=140`, so `multiplier_from_burnout` always returns `MULT_CLUTCH=5.0` at scoring time. Burnout ladder collapses; `SongResults.best_burnout` is meaningless. Blocks the intent of #103.
  - **#176** (AppStore, enhancement): `points = base × timing × burnout × combo` stacks two skill axes multiplicatively; spread Perfect-Dead vs Bad-Safe is 30×, with timing carrying more variance than burnout — opposite of design pillar.
  - **#181** (AppStore, enhancement): `PTS_PER_SECOND=10` and base points are not difficulty-scaled; Easy can outscore Hard. v1 progression hook is broken.
  - **#206** (AppStore, enhancement): `CHAIN_BONUS` flat values are dwarfed by per-obstacle multipliers (~5–15× ratio); chains stop functioning as a real reward axis. Companion to #100.
  - **#211** (AppStore, enhancement): LanePush emplaces `ScoredTag` with no `MissTag`, so `scoring_system` increments `chain_count` for passive auto-pushes; on medium this is up to ~20% of "chain links". Companion to #120.
- Lesson: when filing burnout/timing/chain issues, always cite the exact constant + the line where it composes with the others. The interaction matters more than any single value.
- Lesson: cross-difficulty leaderboard parity (#181) is the AppStore hook Edie cares about — file it as enhancement, not bug, but anchor the impact in player behaviour ("grind Easy").

## 2026-04-28 · #167 Burnout Bank-on-Action Decision
- Decided #167 in favour of **Option 3 (bank-on-action)**: the burnout multiplier is snapshotted on the obstacle at the press/swipe that resolves it, consumed by `scoring_system` instead of the live `BurnoutState`. No-op clears (player already correct, no press) default to Safe ×1.0. Passive `LanePush*` never bank, never write `best_burnout`, never popup, never chain (overlap with #211 — landing #167 first is fine).
- Rejected Option 1 (resolve on zone entry) — would decouple scoring from the press, breaking the existing rhythm timing pipeline. Rejected Option 2 (shrink zones <40 px) — kills the visual gradient and only papers over the resolution-boundary mismatch.
- Architectural note for engineering: `burnout_system.cpp:102-118` zone/meter math should be factored into a `compute_burnout_for_distance(dist, config)` helper so input-time banking and HUD-meter computation stay in sync. New component `BankedBurnout { multiplier, zone }` lives next to `BurnoutState` in `app/components/burnout.h`.
- Lesson: when scoring depends on a transient world state (burnout meter), the *snapshot moment* is itself a design decision, not an implementation detail. The GDD already encoded "bank on action" in prose; the bug is that the codepath never honoured it.
- Lesson: even with bank-on-action, the rhythm timing pipeline forces Perfect-timed presses into the Dead distance band (~88 px at 400 px/s × (morph + half_window)). The ladder becomes *reachable* (which is what #167 asks for) but late-press-Dead remains *strategically dominant*. That dominance is #176's problem, not #167's — keep the issues separated to avoid scope creep.
- Lesson: define "first-commit-locks" up-front for ComboGate/SplitPath. Without it, players are incentivised to wait for higher zones and the design intent ("decide *when* to commit") collapses to "always commit late".

### 2026-05-17 — EnTT ECS Audit: Design Perspective

**Context:** Three-agent audit (Keyser, Keaton, Kujan) completed on ECS compliance and performance patterns. Backlog items assigned across gameplay, UI, and tools teams.

**Design stakeholder notes:**
- **HighScoreState methods (F6):** Mutation logic (e.g., `set_score`) should move to a `high_score_system.cpp` to maintain component-data separation. Affects balance/tuning workflows if high-score persistence requires formula tweaks.
- **SettingsState methods (F6):** Similar pattern; mutation helpers belong in systems. Already flagged in decisions.md as "low urgency."
- **Burnout/Scoring constants:** F2/F3 deduplication (COLLISION_MARGIN, APPROACH_DIST) are infrastructure; no design policy changes needed.

**Status:** Backlog assigned. No design-gate changes needed for remediation. Orchestration log: `.squad/orchestration-log/2026-04-27T19-14-36Z-entt-ecs-audit.md`.

## 2026-05-18 · #324 Burnout ECS Removal — Sign-Off
- **Decision:** APPROVED_FOR_REMOVAL. GDD v1.2 (`game.md` lines 4–9, 79–83) and `feature-specs.md` SPEC 2 header already declared the burnout mechanic dead; #239 collapsed `burnout_system` to a no-op and #167's `BankedBurnout` is now never emplaced. There is no remaining design promise the burnout ECS surface serves.
- **Dead surface (delete):** `BurnoutZone` enum, `BurnoutState`, `BankedBurnout`, `burnout_system` decl + no-op def, `burnout_helpers.h` (`compute_burnout_zone`, `multiplier_for_zone`), constants `MULT_RISKY/DANGER/CLUTCH`, `SongResults::best_burnout` field, and the test files `test_burnout_system.cpp`, `test_burnout_bank_on_action.cpp`, plus the burnout blocks in `test_haptic_system.cpp`/`test_components.cpp`.
- **Must remain (player-facing):** EnergyState/energy_system as the sole survival model; TimingTier + `timing_multiplier` as the only skill multiplier; chain (count, timer, `CHAIN_BONUS`, `max_chain`); distance/time bonus; LanePush exclusion from the scoring ladder (reword its comment to "passive scenery — no scoring", don't drop the branch).
- **Explicit non-goal:** Do NOT add a near-miss bonus or proximity haptic in #324. GDD v1.2 §"Why This Works" intentionally removed the perverse-incentive-to-delay. Any future near-miss work must come as a fresh design proposal.
- **Lesson:** When a mechanic is excised from the GDD, the *first* refactor pass tends to leave behind a `{State, Bank, Zone, helpers, constants, results field}` halo that re-anchors the dead concept across six files. Sign-off issues like #324 are the right moment to enumerate the entire halo, not just the obvious system file — otherwise the next reader assumes the residue is load-bearing and writes new code against it.
- **Lesson:** Keep one positive scoring assertion (`points == floor(base * timing_mult)`) when deleting `test_burnout_bank_on_action.cpp` — that file was the regression net for #239's "scoring is flat 1.0×" promise. Losing the regression coverage entirely would let a future contributor silently reintroduce a multiplier and pass review.
- **Decision file:** `.squad/decisions/inbox/saul-324-burnout-signoff.md`. Implementation owner: McManus.

## 2026-05-10 — Issue #393: prototype.md archived
- Moved `design-docs/prototype.md` → `design-docs/archive/prototype.md` (Option A from triage).
- Strengthened the in-file header: HISTORICAL → ARCHIVED with explicit "do not use" + a curated pointer list to current docs (`game.md`, `rhythm-design.md`, `rhythm-spec.md`, `game-flow.md`).
- Updated direct cross-references that pointed at the old path:
  - `design-docs/game.md` (footer link)
  - `design-docs/game-flow.md` (closing cross-reference)
  - `.github/copilot-instructions.md` (project-overview list)
- Did NOT touch `.squad/log/*` or `.squad/orchestration-log/*` historical entries — those are dated logs and rewriting them would falsify history.
- Did NOT touch unrelated worktree changes (Coordinator energy-tuning / #395 still owned by McManus).
- Learning: when a doc is "marked HISTORICAL" but stays alongside live docs, readers miss the banner. Physically relocating to `archive/` + a replacement-pointer block is more effective than another warning.

## 2026-05-10 · Round 2 Audit Pass (post-PR #408)
- Filed three new design-coherence issues against fresh post-#408 evidence; all dup-checked against #67–#240, #324, #393, #395 backlog.
  - **#420** (squad:saul, enhancement, diagnostic): Across all 9 shipped beatmaps (3 songs × {easy, medium, hard}, 597 obstacles), only 2/3 shapes and 2/3 lanes appear — **circle 0%, lane 2 0%**. Worse, the data is fused: every triangle is in lane 0, every square is in lane 1. Root-caused to `tools/level_designer.py:64-67` `ONSET_CLASS_TO_OBSTACLE` mapping where `harmonic → lane 2 + circle` is intact but the analysis pipeline never emits `layer == "harmonic"` events. Supersedes / sharpens #136 (which described "underrepresentation"; reality is full absence post-#408).
  - **#421** (squad:saul, documentation, diagnostic): GDD `game.md` Difficulty Progression table only has Easy and Hard rows. Medium is a real shipping difficulty with its own contracts (per #135 sign-off) and content footprint (43/85/70 obstacles); the GDD reads as if it doesn't exist. Also flagged that the Hard row's "+ Low/high bars, combos, split paths" is itself stale because shipped Hard is 100% shape_gate.
  - **#422** (squad:saul, documentation, diagnostic): `design-docs/beatmap-design-guidelines.md` predates the rhythm-first rewrite — recommends "60% shape / 20% lane / 10% jump / 10% combo" target which contradicts #135's `LANEPUSH_RAMP["easy"] = None` and shipped reality (100% shape_gate). Also includes a per-song offset-calibration note (`offset += 0.07`) made obsolete by the BPM-derived window in `rhythm-spec.md`. Recommended same fix as #393: move to `archive/` + replacement-pointer banner.

### Lessons
- **Aggregate stats > spot-check.** The Round 1 sweep cited `2_drama medium` shape imbalance (#142). Aggregating across all 9 maps revealed a stronger story: it isn't *one map* that drifts, it's the entire onset→shape/lane mapping collapsing one of three layers. Round 2 audits should always start with a cross-song aggregate before diving per-song.
- **Old issues need data-anchored rechecks, not just status checks.** #136 sat open while the underlying numbers got *worse*, but the title still read "underrepresented" — anyone glancing at the title would not realise it's now 0%. Rule for filing: when reopening or referencing an existing issue, re-quantify and supersede with a sharper title/body if the regime has changed.
- **In-place "HISTORICAL" banners get missed; archive + pointer is the proven pattern.** Reusing the #393 prototype.md template for #422 is faster than relitigating "should we archive?" each time.
- **Mapping tables and emission tables drift apart silently.** `ONSET_CLASS_TO_OBSTACLE` is a 3-class lookup, but as long as the analysis side only emits 2 of those classes, the design space *looks* fine in code but is dead in shipped content. Worth a recurring check that each named class in such tables has non-zero emissions in shipped artifacts.

## 2026-05-10 · Round 3 Audit Pass (post-PR #427)
- Re-ran the cross-song aggregate after #420/#421/#422 landed. Coverage is healed — all 3 shapes and 3 lanes now appear across the 994 obstacles in shipped beatmaps. But the new aggregate exposed a **new** structural finding: every triangle is in lane 0, every square in lane 1, every circle in lane 2. Zero cross-pairs. The Round 2 fix achieved coverage by hard-coding `ONSET_CLASS_TO_OBSTACLE` to a single (lane, shape) per onset class, so shape variety and lane variety are now the same axis and strafing is mechanically dead during shape gates.
- Filed four design-coherence issues (all dup-checked against #44–#440):
  - **#441** (squad:saul, squad:verbal, diagnostic): shape↔lane fusion (994/994 on the diagonal). Asks the design call: decouple lane from onset class, or formally collapse "lane as a skill axis" in the GDD. Either way the docs and content must agree.
  - **#446** (squad:saul, squad:verbal, documentation): game.md Difficulty Progression Medium row still says "Shape gates + lane_push pacing inserts" but shipped content is 0 lane_push across all 9 maps. Hard already has a § footnote acknowledging this; Medium does not. Companion finding: difficulty axis collapses to **density only** in current content (knock-on for #181, #104).
  - **#450** (squad:saul, squad:verbal, documentation): `design-docs/energy-bar.md` was not synced after PR #408 fixed #395. Doc still declares `ENERGY_DRAIN_BAD = 0.10f` and uses 0.10 in the flow diagram and in all four worked tuning scenarios; ship constant is 0.05f. Pure doc desync; risk is a future contributor "fixing" the constant back to 0.10 to match the spec.
  - **#453** (squad:saul, squad:verbal, documentation): game.md §HUD Elements still says "Center field: Proximity ring around player". Every other rhythm-first doc (rhythm-spec §6, rhythm-design §The Proximity Ring, game-flow §FTUE Run 4) and the impl (`gameplay_hud_screen_controller.cpp:142-163`) place the ring around the **shape buttons** at the bottom HUD. Holdover from the burnout-era HUD; one-line surgical edit.

### Lessons
- **Coverage fixes can introduce binding regressions.** When #420 added all three onset classes, the obvious shape-distribution stat went from "broken" to "fine", which masked the fact that the lookup is single-valued per class. Round-N audits need to look at *cross-tabs* (shape×lane) not just marginals.
- **Mechanic-removal banners need to cover whole sections, not just headlines.** The §HUD bullet in game.md survived the v1.2 burnout-removal pass even though the cue's *location* was tightly coupled to the deleted mechanic. When a mechanic is excised, every paragraph that mentions a specific HUD element of the old mechanic must be re-read, even if the element name is generic ("ring", "meter") and survives by coincidence.
- **Constant changes need a doc-sync checklist.** #395 → #408 changed one constant in `app/constants.h`; the doc cited that exact value in five places. A two-minute `grep -n 'ENERGY_DRAIN_BAD\|0.10\|0.05' design-docs/energy-bar.md` would have caught this in the original PR. Worth adding to the design-doc author handoff checklist.
- **"Density-only" difficulty needs to be named.** Once you strip lane_push, bars, combos, and split paths from shipping, the only axis left is obstacle count. That's not nothing — but the GDD doesn't *say* it, so contributors keep filing follow-on issues (#104, #181) that re-derive the same gap. Naming the regime explicitly in `game.md` ("today: density only — see #446") would dedup half the future symptom-issues.
