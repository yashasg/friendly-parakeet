# SHAPESHIFTER — Game Design Document
## v1.1

---

## Concept

Endless runner (like Temple Run) where the player shifts between 3 geometric shapes to pass through matching obstacles. The core twist: a **"Burnout" scoring system** rewards players for waiting until the last possible moment before acting — the longer you delay, the higher your score multiplier, but wait too long and you crash.

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

## Core Mechanic: Burnout Scoring

The defining mechanic. Inspired by "burnout" in racing games.

### How It Works

1. When an obstacle appears on screen and approaches the player, a **Burnout Meter** begins filling.
2. The meter goes through zones: **SAFE → RISKY → DANGER → DEAD**
3. The player can act (switch shape or swipe) at any time to clear the obstacle and **bank** the current burnout multiplier.
4. The longer they wait, the higher the multiplier:
   - Switch immediately: ×1.0 (no bonus)
   - Wait a bit: ×1.5
   - Danger zone: ×3.0
   - Last possible frame: ×5.0 (MAX)
   - Too late: 💀 DEAD (game over)
5. Final points = **base points × burnout multiplier**

### Why This Works

- **Beginners** switch early → safe, low score
- **Good players** push into danger → higher scores
- **Masters** dance on the edge → legendary scores
- Two players at the same distance can have wildly different scores — that's the leaderboard hook.

---

## Obstacle Types

| Obstacle       | Player Action Required    | Burnout? | Base Points |
|---------------|---------------------------|----------|-------------|
| Shape Gate     | Tap correct shape button  | YES      | 200         |
| Lane Push Left | None (auto-pushes player) | NO       | 0           |
| Lane Push Right| None (auto-pushes player) | NO       | 0           |
| Low Bar        | Swipe up (jump)           | YES      | 100         |
| High Bar       | Swipe down (slide)        | YES      | 100         |
| Combo Gate     | Shape + swipe (2 actions) | YES (×2) | 200         |
| Split Path     | Shape + correct lane      | YES (×2) | 300         |

> **Note:** Lane Push replaces the legacy Lane Block. It is a passive obstacle that
> auto-pushes the player one lane in the indicated direction on beat arrival.
> Edge pushes (left on Lane 0, right on Lane 2) are no-ops.

### Combo Obstacles

When an obstacle requires TWO actions (e.g., switch to ● AND swipe left), both burnout timers run independently. The multiplier is the average of both, with a ×2 combo bonus applied.

---

## Scoring

### Points Per Obstacle
- Base points (see table above) × burnout multiplier

### Chain Bonus (flat)
- 2 obstacles in a row: +50
- 3 obstacles in a row: +100
- 4 obstacles in a row: +200
- 5+ obstacles: +100 per extra

### Distance Bonus
- +10 points per second survived (passive income)

### Burnout Popup Feedback
| Multiplier | Text         | Visual Effect              |
|-----------|--------------|----------------------------|
| ×1.0      | (nothing)    | None                       |
| ×1.5      | "Nice"       | Small text, subtle         |
| ×3.0      | "CLUTCH!"    | Large text, orange, shake  |
| ×5.0      | "LEGENDARY!" | Massive, rainbow, explosion|

---

## Difficulty Progression

Speed increases over time via a continuous ramp (`SPEED_RAMP_RATE = 0.011f`), not discrete jumps. The table below shows approximate speed at each phase. Everything gets faster and denser.

| Time      | Speed | New Obstacles Introduced     | Burnout Window |
|-----------|-------|------------------------------|----------------|
| 0–30s     | ×1.0  | Shape gates only, far apart  | Very generous  |
| 30–45s    | ×1.3  | + Lane pushes                | Generous       |
| 45–90s    | ×1.6  | + Low bars, closer spacing   | Moderate       |
| 90–120s   | ×2.0  | + Combos, 2-obstacle chains  | Tighter        |
| 120–150s  | ×2.3  | + Split paths, 3-chains      | Tight          |
| 150–180s  | ×2.6  | + Rapid sequences, all mixed | Very tight     |
| 180s+     | ×3.0  | Full chaos, pure skill zone  | Razor thin     |

### Player Emotion Arc
- **Early** → "I'm learning, this is fun" (confidence)
- **Mid** → "I'm GREEDY for burnout points" (risk/reward dopamine)
- **Late** → "JUST SURVIVE" (adrenaline, flow state)
- **Death** → "ONE MORE TRY" (instant restart, zero friction)

---

## Game Loop

1. Player taps to start. Character begins auto-running forward.
2. Obstacles approach. Burnout meter begins filling.
3. Player acts (tap shape / swipe) to clear the obstacle.
   - Points = base × burnout multiplier
   - Burnout meter resets.
4. If player doesn't act in time → crash → game over.
5. Speed increases over time. Obstacles get denser and more complex.
6. Game over screen shows: final score, distance, best burnout achieved, total shapes shifted.
7. Retry (instant) or return to menu.

---

## HUD Elements

- **Top left**: Current score + best score
- **Top bar**: Speed indicator (progress bar + multiplier)
- **Bottom**: Burnout meter (fills as obstacle approaches)
- **Bottom**: 3 shape buttons (currently selected is highlighted)

---

## References

- **Temple Run** — endless runner feel, speed ramp, lane-based dodging
- **Burnout (racing)** — risk/reward timing mechanic inspiration
- **Geometry Dash** — shape-based obstacle matching

---

*See `prototype.md` for full ASCII art prototype with frame-by-frame gameplay scenarios.*
