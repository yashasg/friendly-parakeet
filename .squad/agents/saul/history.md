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
