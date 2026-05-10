# SHAPESHIFTER — Game Design Document
## v1.2

> **v1.2 change:** The legacy "Burnout" risk/reward scoring mechanic has been
> removed. The game is rhythm-first: timing against the beat is the scoring
> axis. Shape changes that land on the beat are valid play, regardless of
> whether an obstacle is currently near arrival. See `rhythm-design.md` and
> `rhythm-spec.md` for the authoritative rhythm scoring model.

---

## Concept

Rhythm runner where the player shifts between 3 geometric shapes to pass through matching obstacles in time with the music. **Obstacles ARE the beats.** The core skill is hitting the right shape on the beat — closer to the beat means a higher timing grade and a higher score. Stay clean, build a chain, survive the song.

---

## Platform

- **Mobile** — portrait mode only
- Touch controls (tap + swipe)

---

## Player Shapes (3 active + Hexagon default)

| Shape    | Symbol | Passes Through       |
|----------|--------|----------------------|
| Circle   | ●      | Round holes/gates    |
| Square   | ■      | Square holes/gates   |
| Triangle | ▲      | Triangle holes/gates |
| Hexagon  | ⬡      | None (default/rest)  |

The player is always one of these shapes. Hexagon is the default resting shape — the player returns to it between obstacles. Hexagon does not pass through any gates; any gate arriving while in Hexagon = MISS. Tap a shape button to switch instantly.

---

## Controls

### Screen Layout

The screen is divided into two zones:

- **Top ~80%** — SWIPE ZONE (gameplay view). Swipe anywhere in this area:
  - Swipe LEFT → strafe to the left lane
  - Swipe RIGHT → strafe to the right lane
  - Swipe UP → jump
  - Swipe DOWN → slide

- **Bottom ~20%** — BUTTON ZONE. Three shape buttons:
  - `[ ● ]` Circle
  - `[ ■ ]` Square
  - `[ ▲ ]` Triangle

  The currently active shape button is highlighted/glowing. Tapping a button immediately shifts the player to that shape.

### Lanes

3 lanes: left, center, right. Player starts in center.

> ⚠️ **Current shipped behavior — see issue #441.** Across all 9 shipped
> beatmap arrays (**1046 obstacles total** as of Round 6 audit; see
> the per-difficulty table in `rhythm-design.md` §8), shape and lane
> are fused 1:1 by
> `tools/level_designer.py:ONSET_CLASS_TO_OBSTACLE` — every triangle is
> on lane 0, every square on lane 1, every circle on lane 2. As a
> result, **strafing input is currently redundant**: choosing the
> correct shape implies the correct lane, and a player who never strafes
> still passes 100% of shape gates. The Controls section above and the
> Split Path entry in the Obstacle Types table reflect the intended
> design space, not shipped content. The shape↔lane decoupling vs.
> intentional-collapse design call is tracked in #441 and must be
> resolved before lane-as-a-skill claims are reinstated; until then,
> treat lane variety as a known limitation of shipped beatmaps.

---

## Core Mechanic: On-Beat Shape Matching

The defining mechanic is rhythm timing. Every obstacle is anchored to a beat in the song. The player's job is to be in the correct shape (and lane) at the moment the obstacle reaches the player line — and to commit that input as close to the beat as possible.

### How It Works

1. The song plays. The beat scheduler emits obstacles whose arrival times line up with beats.
2. The player presses shape buttons and changes lanes **in time with the music**. There is no "wait until the obstacle is near" pressure: a shape change made on a beat is valid play, even if no obstacle is currently arriving.
3. When an obstacle resolves, the system grades the timing of the resolving input relative to the beat:
   - **Perfect** — closest to the beat (highest multiplier)
   - **Good**
   - **Ok**
   - **Bad** — furthest from the beat that still counts as a hit
   - **Miss** — wrong shape/lane at arrival, or no input in window
4. Points scale with the timing grade and with the active chain. There is no "delay until the last frame" risk/reward — earlier or later than the beat both lose points symmetrically.

### Why This Works

- The skill expression is *staying on rhythm*, not *flirting with death*.
- Two players hitting the same notes can still differentiate on score via timing precision and chain length.
- Removing burnout removes the perverse incentive to delay shape changes; players are free to change shape on any beat that feels musical.

See `rhythm-design.md` for the full timing-window grading table and `rhythm-spec.md` for the BPM-derived window math.

---

## Obstacle Types

| Obstacle        | Player Action Required    | Timed? | Base Points |
|-----------------|---------------------------|--------|-------------|
| Shape Gate      | Tap correct shape button  | YES    | 200         |
| Low Bar         | Swipe up (jump)           | YES    | 100         |
| High Bar        | Swipe down (slide)        | YES    | 100         |
| Combo Gate      | Shape + swipe (2 actions) | YES    | 200 (×2)    |
| Split Path      | Shape + correct lane      | YES    | 300 (×2)    |

### Combo Obstacles

When an obstacle requires TWO actions (e.g., switch to ● AND swipe left), the timing grade for each input is computed independently and the combo bonus stacks on top of the base points.

---

## Scoring

### Points Per Obstacle
- `floor(base_points × timing_multiplier) + chain_flat_bonus`
- Timing multiplier comes from the beat-distance grade (Perfect/Good/Ok/Bad). See `rhythm-design.md`.

### Chain
- Each consecutive non-miss timing grade (Perfect, Good, Ok, or Bad) grows the chain.
- Bad awards reduced points and drains small energy. Miss resets the chain.
- Chain resets on any MISS.
- Chain contributes a bounded flat bonus so consistent rhythm is rewarded over isolated hits.

### Distance Bonus
- +10 points per second survived (passive income).

### Timing Popup Feedback
| Grade   | Text         | Visual Effect              |
|---------|--------------|----------------------------|
| Bad     | (subtle)     | Small text                 |
| Ok      | "OK"         | Small text                 |
| Good    | "GOOD"       | Medium text                |
| Perfect | "PERFECT!"   | Large text, bright, shake  |
| Miss    | "MISS"       | Red flash, energy drain    |

---

## Difficulty Progression

Difficulty is selected per song (easy / medium / hard) and is expressed primarily through obstacle density, kind variety, and lane complexity in the beatmap. Within a song, scroll speed is BPM-derived and constant — there is no in-song speed ramp.

| Difficulty | Obstacles Introduced | Density | Timing Window† |
|------------|----------------------|---------|----------------|
| Easy       | Shape gates only     | Low     | BPM-derived (shared) |
| Medium     | Shape gates only‡    | Medium  | BPM-derived (shared) |
| Hard       | Shape gates only‡    | High    | BPM-derived (shared) |

† Timing windows are not difficulty-scaled today: every difficulty uses the same BPM-derived hit window defined in `rhythm-spec.md` (`audio_offset` + BPM-derived window). The "Timing Window" column is preserved for forward compatibility if per-difficulty scaling is reintroduced.

‡ As shipped today (see #420, #446), all 9 beatmaps in `content/beatmaps/` are 100% `shape_gate`. Difficulty between Easy / Medium / Hard is expressed only through **obstacle density**, **shape-gate kind/lane distribution**, and **onset selection** — not through additional obstacle kinds. The earlier `lane_push` pacing-insert plan (decisions.md "#135 — Difficulty Ramp", `LANEPUSH_RAMP`) and the older "+ Low/high bars, combos, split paths" cell for Hard were aspirational; no `lane_push` / `low_bar` / `high_bar` / `combo_gate` / `split_path` content is currently produced by `level_designer.py`, and `LanePush` is queued for removal/rework (#328). Those obstacle kinds remain in the catalog as future design space; if/when a committed plan to reintroduce them lands, this table will be revised.

### Player Emotion Arc
- **Early song** → "I'm finding the rhythm" (entrainment)
- **Mid song** → "I'm IN the groove" (flow, chain pride)
- **Late / chorus** → "Don't break the chain" (focus, stakes)
- **Death** → "ONE MORE TRY" (instant restart, zero friction)

---

## Game Loop

1. Player picks a song and difficulty. Song begins playing.
2. Obstacles approach in time with the music.
3. Player presses shape buttons and changes lanes on the beat.
   - On-beat shape changes are valid even when no obstacle is arriving.
   - When an obstacle resolves, the input's distance from the beat determines the timing grade and score multiplier.
4. Hits feed energy and the chain. Misses drain energy. When energy hits zero → game over.
5. Song Complete shows final score, hit breakdown (Perfect/Good/Ok/Bad/Miss), best chain, and energy remaining. Game Over ships with final score, high score, and terminal reason; the fuller stat panel is future scope.
6. Retry (instant) or return to song select.

---

## HUD Elements

> **Source of truth for layout:** `content/ui/screens/gameplay.rgl`
> (720×1280 portrait canvas). See also `design-docs/energy-bar.md`
> for the energy-bar spec. If this list drifts from the `.rgl`, the
> `.rgl` wins.

- **Top left**: Current score (`ScoreSlot`) and best score
  (`HighScoreSlot`), stacked vertically.
- **Top right**: **Pause button** (`PauseButton`, glyph `||`,
  ~80×50 px at x=620, y=10) — shipped on-screen pause affordance.
  Tapping it transitions `Playing → Paused` (overlay); see
  `game-flow.md` §1 "Master Screen Flow Map" / Pause Screen, and
  the historical "no manual pause" complaint in #144 (now resolved
  by this button).
- **Left edge — vertical Energy bar** (`EnergyBarSlot`, ~30 px wide
  × 200 px tall, anchored at x=10, y=790) with an **"ENERGY" text
  label** (`EnergyLabel`) directly above it (x=10, y=745). The bar
  drains on miss and recovers on hit. The label addresses the
  label-less-bar complaint in #171; both are shipped today.
- **Bottom**: 3 shape buttons (`ShapeButtonCircle`,
  `ShapeButtonSquare`, `ShapeButtonTriangle`, each ~140×100 px on
  the y=1140 row, separated from the play field by `LaneDividerSlot`
  at y=1120). Currently selected button is highlighted, and each is
  wrapped by a **proximity ring** that shrinks toward the button as
  the matching obstacle approaches, providing the live timing cue
  (see `rhythm-spec.md` §6 / `rhythm-design.md` §4).

> Any future redesign that wants to relocate the energy bar to the
> top of the HUD must land as an explicit change to
> `gameplay.rgl` + this section; today the bar is vertical on the
> left edge.

---

## References

- **Beat Saber / Cytus II** — rhythm-first scoring, timing windows
- **Geometry Dash** — shape-based obstacle matching, music-synced levels
- **Temple Run** — three-lane endless-runner feel

---

*For current rhythm-first scoring and gameplay, see `rhythm-design.md` and `rhythm-spec.md`. The legacy burnout-era ASCII prototype has been archived to `archive/prototype.md` (do not use for current design — see issue #393).*
