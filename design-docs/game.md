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

## Player Shapes (3)

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
| Lane Push Left  | None (auto-pushes player) | NO     | 0           |
| Lane Push Right | None (auto-pushes player) | NO     | 0           |
| Low Bar         | Swipe up (jump)           | YES    | 100         |
| High Bar        | Swipe down (slide)        | YES    | 100         |
| Combo Gate      | Shape + swipe (2 actions) | YES    | 200 (×2)    |
| Split Path      | Shape + correct lane      | YES    | 300 (×2)    |

> **Note:** Lane Push is a passive obstacle that auto-pushes the player one lane
> in the indicated direction on beat arrival. Edge pushes (left on Lane 0, right
> on Lane 2) are no-ops. Lane Pushes do not contribute to chain or score.

### Combo Obstacles

When an obstacle requires TWO actions (e.g., switch to ● AND swipe left), the timing grade for each input is computed independently and the combo bonus stacks on top of the base points.

---

## Scoring

### Points Per Obstacle
- `base_points × timing_multiplier × chain_bonus`
- Timing multiplier comes from the beat-distance grade (Perfect/Good/Ok/Bad). See `rhythm-design.md`.

### Chain
- Each consecutive HIT (Perfect, Good, or Ok) grows the chain.
- Chain resets on any MISS.
- Chain contributes a bounded multiplier so consistent rhythm is rewarded over single big hits.

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

| Difficulty | Obstacles Introduced                 | Density           | Timing Window |
|------------|--------------------------------------|-------------------|---------------|
| Easy       | Shape gates only                     | Low               | Generous      |
| Medium     | + Lane pushes (taught after intro)   | Medium            | Standard      |
| Hard       | + Low/high bars, combos, split paths | High              | Standard      |

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
5. Game over / song complete screen shows: final score, hit breakdown (Perfect/Good/Ok/Bad/Miss), best chain, energy remaining.
6. Retry (instant) or return to song select.

---

## HUD Elements

- **Top left**: Current score + best score
- **Top**: Energy bar (drains on miss, recovers on hit)
- **Center field**: Proximity ring around player — visualises beat timing windows
- **Bottom**: 3 shape buttons (currently selected is highlighted)

---

## References

- **Beat Saber / Cytus II** — rhythm-first scoring, timing windows
- **Geometry Dash** — shape-based obstacle matching, music-synced levels
- **Temple Run** — three-lane endless-runner feel

---

*See `prototype.md` for full ASCII art prototype with frame-by-frame gameplay scenarios.*
