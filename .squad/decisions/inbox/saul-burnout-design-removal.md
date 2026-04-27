# Decision Proposal — Burnout System Removal (issue #239)

**Owner:** Saul (Game Design)
**Status:** PROPOSED — design-doc pass complete; engineering follow-up required
**Issue:** https://github.com/yashasg/friendly-parakeet/issues/239

## Decision

The **Burnout** risk/reward scoring mechanic is removed from the game design.

- It is no longer a core pillar.
- It is no longer a scoring axis (no `× burnout_multiplier` term).
- It is no longer a player-facing system (no burnout meter, no burnout zones, no burnout popups, no "wait for more points" tutorial beat).
- It is no longer a failure path (the burnout DEAD-zone game-over rule is gone; failure is owned solely by the energy bar).

## Rationale

Burnout (the longer you wait, the higher your multiplier) **structurally conflicts with rhythm-on-beat play**. A player who is staying on the beat is, by definition, not "delaying" their input. The two pillars were sending opposite signals:

- *Rhythm pillar:* "Press exactly on the beat. Earlier or later both lose points."
- *Burnout pillar:* "Press as late as you can get away with. The closer to the obstacle, the higher the multiplier."

Diagnostic notes (recorded earlier in `.squad/agents/saul/history.md`) already showed burnout was never doing what the doc claimed it did at the code level — `COLLISION_MARGIN=40` < `ZONE_DANGER_MIN=140` collapsed the multiplier ladder onto a single value. Issue #167 patched the symptom (bank-on-action). #239 removes the cause.

## What Replaces It

The rhythm scoring model already in `rhythm-design.md` and `rhythm-spec.md`:

- **Scoring formula:** `base_points × timing_multiplier × chain_multiplier`
- **Timing multiplier:** Perfect / Good / Ok / Bad, computed from the input's distance to the beat at obstacle resolution.
- **Chain:** consecutive non-Miss hits.
- **Visual cue:** the **proximity ring** around shape buttons (already shipped) is the live timing readout that used to be served by the burnout meter.
- **Failure:** energy bar drain on Miss/Bad (see `energy-bar.md`).

**Crucial player-facing rule:** rhythmically on-beat shape changes are *valid play and not penalised* just because no obstacle is currently near arrival. The player is allowed (and encouraged) to feel the music with their inputs.

## Doc Changes (this pass)

Rewrites:
- `README.md` — removed `burnout` from the system pipeline diagram.
- `design-docs/game.md` (v1.1 → v1.2) — concept, core mechanic, obstacle table, scoring, difficulty, emotion arc, game loop, HUD, references.
- `design-docs/game-flow.md` — added doc-level deprecation banner; **rewrote Tutorial Run 4** from "Risk & Reward — Burnout meter intro" to "Stay on the Beat — On-beat timing intro" using the proximity ring + PERFECT/GOOD/OK/BAD popups; updated Run 5 summary.
- `design-docs/rhythm-design.md` — scoring formula now `base × timing × chain` (dropped `× burnout_mult`).
- `design-docs/energy-bar.md` — removed claim that "burnout multiplier remains the scoring risk/reward mechanic"; HUD slot text updated.
- `design-docs/tester-personas.md` — Pro player validation bullet rewritten ("Timing windows reward skilled on-beat play").
- `design-docs/tester-personas-tech-spec.md` — removed `burnout_system` from system pipeline and dependency chain; `hp_system` → `energy_system`.
- `design-docs/beatmap-integration.md` — pipeline diagram cleaned (burnout/hp removed, energy added); `SongResults.best_burnout` field removed.
- `docs/ios-testflight-readiness.md` — background-preservation table no longer lists burnout; chain count listed instead.
- `docs/sokol-migration.md`, `design-docs/normalized-coordinates.md`, `design-docs/isometric-perspective.md` — incidental burnout mentions cleaned.

Banners (engineering docs preserved as archival):
- `design-docs/architecture.md` — banner at top noting `BurnoutState`/`BurnoutZone`/`BankedBurnout`/`burnout_system`/`BURNOUT_*` constants are historical; rhythm-spec is authoritative.
- `design-docs/feature-specs.md` — banner noting SPEC 2 (Burnout Scoring System) is fully superseded.
- `design-docs/prototype.md` — banner noting BURNOUT meters and ×1→×5 popups in the ASCII frames are archival illustration only.

## Engineering Follow-Up (NOT done in this pass)

This decision is a *design* decision; the doc pass does not touch code or tests. The engineering team owns the corresponding cleanup, which at minimum will need to:

1. Remove `BurnoutState`, `BurnoutZone`, `BankedBurnout` components and `burnout_system.cpp` from `app/`.
2. Remove `multiplier_from_burnout` and the `× burnout_multiplier` term from `scoring_system.cpp`. Scoring becomes `base × timing_grade_multiplier × chain_multiplier`.
3. Remove `BURNOUT_*` constants and the burnout meter HUD draw call from `render_system.cpp`.
4. Remove `SongResults.best_burnout` and any tests/serialization that read it.
5. Drop `[burnout]`, `[burnout_bank]` Catch2 tags and the tests under them; keep `[scoring]` tests but expect the simpler formula.
6. Confirm the `LanePush` no-score path is still respected (now even simpler, with no banking).

Owner suggestion: McManus (gameplay) implements; Verbal/Baer covers tests; Kujan reviews; Saul signs off on player-facing scoring numbers when the new chain-only ladder is tuned.

## Non-Goals

- **No new mechanic** is being introduced to "fill the gap" left by burnout. The proximity ring and chain already carry that weight; adding anything else would re-introduce the same conflict with on-beat play.
- **Difficulty content (#125, #134, #135) is unchanged.** Easy/medium/hard contracts (shape-gate-only easy, taught-LanePush medium, bars on hard) remain intact.
- **Energy bar tuning is not revisited here** — it was tuned independently and continues to be the sole survival meter.
