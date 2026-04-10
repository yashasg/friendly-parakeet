# SHAPESHIFTER — Feature Specifications
## Mid Game Designer Deliverable · v1.0

> Three implementation-ready specs for the Input System, Burnout Scoring,
> and Obstacle Spawning & Difficulty systems.

```
  ┌─────────────────────────────────────────────────┐
  │  SPEC 1: Input System ..................... p.1  │
  │  SPEC 2: Burnout Scoring System .......... p.2  │
  │  SPEC 3: Obstacle Spawning & Difficulty .. p.3  │
  └─────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# SPEC 1 — INPUT SYSTEM
# ═══════════════════════════════════════════════════

## Overview

The Input System translates raw touch events into game actions (lane
changes, jump, slide, shape shifts). It owns zone detection, gesture
recognition, debouncing, and an input buffer so simultaneous/rapid
inputs resolve deterministically.

```
  SCREEN ZONES
  ╔══════════════════════════════╗
  ║                              ║
  ║     SWIPE ZONE  (top 80%)    ║  ← swipe anywhere
  ║     ─────────────────────    ║     LEFT / RIGHT / UP / DOWN
  ║                              ║
  ║                              ║
  ║                              ║
  ║                              ║
  ║                              ║
  ║                              ║
  ║                              ║
  ║                              ║
  ║                              ║
  ║──────────────────────────────║  ← y_split = 0.80 * screen_h
  ║  BUTTON ZONE  (bottom 20%)  ║
  ║   [ ● ]    [ ■ ]    [ ▲ ]   ║  ← tap shape buttons
  ╚══════════════════════════════╝

  SWIPE DETECTION (angle thresholds):

              UP (jump)
               ↑
          315° │ 45°
               │
  LEFT ←───────┼───────→ RIGHT
               │
          225° │ 135°
               ↓
            DOWN (slide)

  Each quadrant spans 90°.
  Dead-zones: none (full 360° coverage).
  Angles measured from positive-X axis, counter-clockwise.
```

## User Stories

- As a player, I want to **swipe left/right** in the top zone so that my character changes lanes.
- As a player, I want to **swipe up** so that my character jumps over low bars.
- As a player, I want to **swipe down** so that my character slides under high bars.
- As a player, I want to **tap a shape button** so that my character shifts to that shape instantly.
- As a player, I want to **swipe AND tap nearly simultaneously** (combo obstacles) and have both actions register.
- As a player, I want **accidental micro-touches** (jitter) to be ignored so I don't misfire.

## Acceptance Criteria

- [ ] Touches that begin at `y >= 0.80 * screen_height` are routed to button detection; all others to swipe detection.
- [ ] A swipe is recognised when `distance >= MIN_SWIPE_DIST` (50 dp) AND `duration <= MAX_SWIPE_TIME` (300 ms).
- [ ] Swipe direction is the dominant-axis direction of the delta vector, using 45° quadrant boundaries.
- [ ] A swipe that does not meet distance OR time thresholds is discarded (no action).
- [ ] A button tap is recognised on touch-up within the same button's hit-rect, with a debounce window of 100 ms.
- [ ] Tapping the **already-active** shape button is a no-op (no event emitted).
- [ ] Two inputs arriving in the same frame (swipe + tap) both resolve: the input buffer holds up to 2 pending actions.
- [ ] Buffered inputs are consumed in order: **shape change first, then movement**, so combo obstacles work.
- [ ] If two swipes or two taps arrive in the buffer, only the **latest** of each type is kept.
- [ ] Multi-touch: at most **2 simultaneous touches** are tracked (one per zone). Third+ fingers are ignored.
- [ ] On raylib touch events, finger positions are converted from screen coords to logical coords before processing.

## Technical Requirements

### ECS Components (C++ structs)

```cpp
// ── Identifies which zone a touch belongs to ──
enum class InputZone : uint8_t {
    Swipe,   // top 80%
    Button   // bottom 20%
};

// ── Raw touch event snapshot, one per active finger ──
struct TouchEvent {
    uint64_t finger_id;          // touch finger ID
    InputZone zone;              // resolved zone at touch-down
    float start_x, start_y;     // normalised → pixel, at touch-down
    float current_x, current_y; // latest position
    float start_time;            // seconds since app start
    bool  active;                // false once finger lifts
};

// ── Recognised swipe gesture (written by gesture_recognition_system) ──
enum class SwipeDirection : uint8_t {
    Left, Right, Up, Down
};

struct SwipeGesture {
    SwipeDirection direction;
    float magnitude;             // distance in dp
    float duration;              // seconds held
    float timestamp;             // when recognised
    bool  consumed;              // true once a gameplay system reads it
};

// ── Shape button tap (written by gesture_recognition_system) ──
enum class ShapeID : uint8_t {
    Circle   = 0,   // ●
    Square   = 1,   // ■
    Triangle = 2    // ▲
};

struct ShapeButtonTap {
    ShapeID shape;
    float   timestamp;
    bool    consumed;            // true once a gameplay system reads it
};

// ── Per-frame input state, singleton component ──
//    Holds the resolved buffer that gameplay systems read.
struct InputState {
    // Buffer: max 1 swipe + 1 tap per frame
    bool              has_swipe;
    SwipeGesture      buffered_swipe;

    bool              has_tap;
    ShapeButtonTap    buffered_tap;

    // Debounce tracking
    float             last_tap_time;      // for 100 ms debounce

    // Active touches (max 2)
    static constexpr int MAX_TOUCHES = 2;
    TouchEvent        touches[MAX_TOUCHES];
    int               active_touch_count;
};
```

### Systems (function signatures)

```cpp
// Reads raylib input, populates / updates InputState::touches[].
// Classifies each touch into Swipe or Button zone.
// Called first in the input phase.
void input_detection_system(entt::registry& reg, float dt);

// Examines completed touches (finger-up), produces SwipeGesture or
// ShapeButtonTap, writes them into InputState buffer.
// Applies min-distance, max-time, debounce, and dedup rules.
// Called second in the input phase.
void gesture_recognition_system(entt::registry& reg, float dt);
```

### Input → Output Flow

```
  raylib touch events (GetTouchPointCount, GetTouchPosition)
         │
         ▼
  ┌──────────────────────┐
  │ input_detection_system│  → updates InputState::touches[]
  └──────────┬───────────┘
             │
             ▼
  ┌──────────────────────────┐
  │ gesture_recognition_system│  → writes InputState::buffered_swipe
  └──────────┬───────────────┘    and/or   ::buffered_tap
             │
             ▼
  ┌──────────────────────┐
  │  gameplay systems     │  → consume .has_swipe / .has_tap
  │  (movement, shape)   │     set .consumed = true
  └──────────────────────┘
```

### Dependencies

- **raylib** — touch input via `GetTouchPointCount()`, `GetTouchPosition()`, mouse via `IsMouseButtonPressed/Released()`
- **No dependency on other feature specs** — Input is the lowest-level system.

### Edge Cases

| Case | Resolution |
|------|------------|
| Swipe starts in swipe zone, finger drags into button zone | Zone is locked at **touch-down**. Still treated as swipe. |
| Player swipes < 50 dp (jitter) | Discarded, no action. |
| Player holds finger > 300 ms then lifts | Not a swipe (duration exceeded). Discarded. |
| Two fingers both in swipe zone | Second touch ignored (one swipe at a time). |
| Tap on boundary between two buttons | Hit-test uses button center ± half-width. Nearest-center wins. |
| Rapid double-tap on same button | Debounce: second tap within 100 ms is dropped. |
| Tap on already-selected shape | No event emitted. |
| Simultaneous swipe + tap (combo) | Both buffer slots filled; shape change processed first. |

## Balancing Parameters

```
  ┌───────────────────────────────────────────────────────────┐
  │  PARAMETER              │  DEFAULT   │  NOTES             │
  ├─────────────────────────┼────────────┼─────────────────────│
  │  ZONE_SPLIT_Y           │  0.80      │  fraction of screen │
  │  MIN_SWIPE_DIST_DP      │  50        │  density-indep px   │
  │  MAX_SWIPE_TIME_SEC     │  0.300     │  seconds            │
  │  TAP_DEBOUNCE_SEC       │  0.100     │  seconds            │
  │  BUTTON_HIT_PADDING_DP  │  12        │  expand hit-rect    │
  │  MAX_SIMULTANEOUS_TOUCH │  2         │  fingers tracked    │
  │  INPUT_BUFFER_SIZE      │  2         │  1 swipe + 1 tap    │
  └───────────────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# SPEC 2 — BURNOUT SCORING SYSTEM
# ═══════════════════════════════════════════════════

## Overview

Burnout is the core risk/reward mechanic. As an obstacle approaches, a
meter fills from 0 → 100%. The later the player acts, the higher their
score multiplier — but cross 100% and it's game over. This spec covers
meter fill logic, zone thresholds, multiplier banking, chain bonuses,
and passive distance scoring.

```
  THE BURNOUT METER (visual model)

  0%                     40%       70%     95% 100%
  ├───── SAFE ────────────┼── RISKY ─┼─ DANGER ┼─☠─┤
  │ ░░░░░░░░░░░░░░░░░░░░ │ ▓▓▓▓▓▓▓▓ │ ████████│☠☠ │
  │        x1.0           │   x1.5   │x2→x3→x5│DIE│
  └───────────────────────┴──────────┴─────────┴───┘

  MULTIPLIER RAMP (linear interpolation within each zone):

  mult
  5.0 ┤                                          ╱☠
      │                                        ╱
  3.0 ┤                                  ····╱
      │                            ····
  2.0 ┤                      ····
      │                ····
  1.5 ┤          ····
      │    ····
  1.0 ┤────
      └──────────────────────────────────────────→ %
      0%   10%  20%  30%  40%  50%  60%  70% 95% 100%
```

## User Stories

- As a player, I want to **see a burnout meter fill** as an obstacle approaches so I can judge my risk.
- As a player, I want to **act (shape-shift or swipe) to bank my score** and lock in the current multiplier.
- As a player, I want **higher multipliers the longer I wait** so there's a reason to push my luck.
- As a player, I want to **die if I wait too long** so the tension feels real.
- As a player, I want **chain bonuses** for clearing multiple obstacles without dying so streaks feel rewarding.
- As a player, I want **passive distance points** so I always feel like I'm earning something.

## Acceptance Criteria

- [ ] The burnout meter is driven by the **nearest relevant obstacle** — the one closest to the player that the player has not yet cleared.
- [ ] Meter fill is **linear with inverse distance**: `fill = 1.0 - (obstacle_dist / detection_range)`, clamped to `[0, 1]`.
- [ ] Zone thresholds: `SAFE [0.0, 0.40)`, `RISKY [0.40, 0.70)`, `DANGER [0.70, 0.95)`, `DEAD [0.95, 1.0]`.
- [ ] Multiplier is interpolated per-zone:
  - SAFE: `lerp(1.0, 1.5, fill / 0.40)`
  - RISKY: `lerp(1.5, 2.0, (fill - 0.40) / 0.30)`
  - DANGER: `lerp(2.0, 5.0, (fill - 0.70) / 0.25)`
  - DEAD: game over triggered.
- [ ] When player acts correctly, points are **banked**: `floor(base_points * multiplier)`.
- [ ] After banking, the meter resets to 0 and begins tracking the next nearest obstacle.
- [ ] Combo obstacles (2 required actions) use the **average multiplier** of both actions, then apply a x2 combo bonus: `floor(base_points * avg_mult * 2)`.
- [ ] If multiple obstacles are on-screen, the meter tracks the **closest** one. Others queue behind.
- [ ] Chain counter increments on each successful clear, resets to 0 on death.
- [ ] Chain bonus is flat additive: `2→+50, 3→+100, 4→+200, 5+→+100 per extra`.
- [ ] Distance score accumulates at `+10 points per second`, unaffected by burnout multiplier.
- [ ] Burnout `DEAD` zone crossing triggers `GameOver` tag on the player entity.

## Technical Requirements

### ECS Components (C++ structs)

```cpp
// ── Zone enum for UI / feedback ──
enum class BurnoutZone : uint8_t {
    Safe,    // 0.00 – 0.40
    Risky,   // 0.40 – 0.70
    Danger,  // 0.70 – 0.95
    Dead     // 0.95 – 1.00
};

// ── Singleton: the burnout meter state ──
struct BurnoutMeter {
    float fill;                  // 0.0 → 1.0
    float multiplier;            // computed each frame (1.0 → 5.0)
    BurnoutZone zone;            // current zone enum
    entt::entity tracked_obstacle; // which obstacle is driving the meter
    bool  active;                // false if no obstacle in range
};

// ── Singleton: player's score ──
struct Score {
    int   total;                 // running total displayed on HUD
    int   last_banked;           // most recent obstacle's banked points
    float last_multiplier;       // for popup feedback text
    float distance_accumulator;  // fractional seconds for +10/sec
};

// ── Singleton: chain tracking ──
struct ChainTracker {
    int   current_chain;         // consecutive clears (resets on death)
    int   best_chain;            // session best (for game-over screen)
    int   last_chain_bonus;      // flat bonus awarded this clear
};

// ── Per-obstacle component: how close it is to the player ──
struct ObstacleProximity {
    float distance;              // world units to player
    float detection_range;       // distance at which meter begins
    bool  cleared;               // true once player successfully passes
};

// ── Tag: applied to player entity on burnout death ──
struct GameOver {};

// ── Feedback event: emitted on bank for UI popups ──
struct BurnoutPopup {
    float       multiplier;
    int         points;
    const char* text;            // "Nice", "GREAT!", "CLUTCH!", etc.
};
```

### Systems (function signatures)

```cpp
// Finds the nearest un-cleared obstacle, computes fill & multiplier.
// Writes BurnoutMeter singleton every frame.
// If fill >= 0.95 and player hasn't acted, emplace GameOver tag.
void burnout_fill_system(entt::registry& reg, float dt);

// On player action (input consumed), reads current BurnoutMeter,
// computes banked points, updates Score, emits BurnoutPopup.
// Resets meter and marks obstacle as cleared.
void score_banking_system(entt::registry& reg, float dt);

// After each bank, increments ChainTracker::current_chain.
// Computes flat chain bonus and adds to Score::total.
// Resets chain on GameOver.
void chain_tracking_system(entt::registry& reg, float dt);

// Adds 10 * dt to Score::distance_accumulator, awards 10 pts
// each time accumulator >= 1.0.
void distance_score_system(entt::registry& reg, float dt);
```

### Burnout Fill Pipeline

```
  EVERY FRAME:
  ┌──────────────────────────────────────────────┐
  │  1. burnout_fill_system                      │
  │     ├─ find nearest obstacle with            │
  │     │  ObstacleProximity.cleared == false     │
  │     ├─ fill = 1 - (dist / detection_range)   │
  │     ├─ clamp fill to [0, 1]                  │
  │     ├─ compute zone + multiplier             │
  │     └─ if fill >= 0.95 → GameOver            │
  └──────────────┬───────────────────────────────┘
                 │
  ON PLAYER ACTION (input consumed):
  ┌──────────────┴───────────────────────────────┐
  │  2. score_banking_system                     │
  │     ├─ pts = floor(base * multiplier)        │
  │     ├─ Score.total += pts                    │
  │     ├─ mark obstacle cleared                 │
  │     └─ emit BurnoutPopup                     │
  ├──────────────────────────────────────────────┤
  │  3. chain_tracking_system                    │
  │     ├─ chain++ → compute chain bonus         │
  │     └─ Score.total += chain_bonus            │
  ├──────────────────────────────────────────────┤
  │  4. distance_score_system                    │
  │     └─ Score.total += 10 per elapsed second  │
  └──────────────────────────────────────────────┘
```

### Dependencies

- **Obstacle Spawning & Difficulty (Spec 3)** — obstacles must exist with `ObstacleProximity` components.
- **Input System (Spec 1)** — `score_banking_system` reads `InputState` to detect player action.

### Edge Cases

| Case | Resolution |
|------|------------|
| Two obstacles same distance | Pick the one with the **lower entity ID** (deterministic). |
| Player acts with no obstacle in range | No-op. Meter is inactive, no points banked. |
| Obstacle passes player without action | `fill` hits 1.0 → `DEAD` zone → `GameOver`. |
| Combo obstacle: player does action 1, then dies on action 2 | First action banks at its multiplier. Second drives a new fill; if that hits DEAD, game over. |
| Chain resets | Only on `GameOver`. Surviving resets nothing. |
| Multiplier exactly at zone boundary (e.g. fill = 0.40) | Belongs to the **higher** zone (RISKY). |
| Distance scoring during pause/menu | `dt` is 0 while paused; no points accumulate. |

## Balancing Parameters

```
  ┌──────────────────────────────────────────────────────────────┐
  │  PARAMETER                │  DEFAULT  │  NOTES               │
  ├───────────────────────────┼───────────┼───────────────────────│
  │  SAFE_THRESHOLD           │  0.40     │  fill %               │
  │  RISKY_THRESHOLD          │  0.70     │  fill %               │
  │  DANGER_THRESHOLD         │  0.95     │  fill %  (≥ = DEAD)   │
  │  MULT_SAFE_MIN            │  1.0      │  x at fill=0          │
  │  MULT_SAFE_MAX            │  1.5      │  x at fill=0.40       │
  │  MULT_RISKY_MAX           │  2.0      │  x at fill=0.70       │
  │  MULT_DANGER_MAX          │  5.0      │  x at fill=0.95       │
  │  DETECTION_RANGE_BASE     │  12.0     │  world units          │
  │  CHAIN_BONUS_2            │  50       │  flat pts             │
  │  CHAIN_BONUS_3            │  100      │  flat pts             │
  │  CHAIN_BONUS_4            │  200      │  flat pts             │
  │  CHAIN_BONUS_5_PLUS_EACH  │  100      │  flat pts per extra   │
  │  DISTANCE_PTS_PER_SEC     │  10       │  passive income       │
  │  COMBO_MULT_BONUS         │  2.0      │  applied after avg    │
  └──────────────────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# SPEC 3 — OBSTACLE SPAWNING & DIFFICULTY
# ═══════════════════════════════════════════════════

## Overview

This system spawns obstacle entities ahead of the player, ramps
difficulty (speed, density, type variety, burnout window) over a 180-
second arc, and cleans up obstacles that have scrolled past the player.

```
  SPAWN PIPELINE (top-down view of the 3-lane track)

                    SPAWN HORIZON  (off-screen, ahead)
  ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─
        │         │         │
        │  ╔═══╗  │         │     newly spawned gate
        │  ║ ● ║  │         │
        │  ╚═══╝  │         │
        │         │         │
        │         │  ╔═══╗  │     queued obstacle
        │         │  ║ X ║  │
        │         │  ╚═══╝  │
        │         │         │
        │         │         │
        │         │   ■     │     ← player
        │         │ (you)   │
  ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─
                    CLEANUP LINE  (off-screen, behind)


  DIFFICULTY ARC (speed multiplier over time)

  speed
  3.0x ┤                                        ╱────
       │                                   ···╱
  2.6x ┤                              ···
       │                         ···
  2.3x ┤                    ···
       │               ···
  2.0x ┤          ···
       │     ···
  1.6x ┤   ·
  1.3x ┤ ·
  1.0x ┤─
       └──────────────────────────────────────────→ t
       0s   30s  60s  90s  120s 150s 180s      ∞
```

## User Stories

- As a player, I want **obstacles to appear gradually** at first so I can learn the mechanics.
- As a player, I want **new obstacle types to unlock over time** so the game stays fresh.
- As a player, I want **the game to get faster and denser** so I feel challenged.
- As a player, I want **fair obstacle placement** (no impossible combos, no 3 identical blocks in one lane).
- As a player, I want the **burnout window to shrink** as I improve so burnout stays risky at all skill levels.

## Acceptance Criteria

- [ ] Obstacles spawn at a fixed **distance ahead** of the player (spawn horizon), not on a timer.
- [ ] Spawn is triggered when the distance from the player to the last spawned obstacle exceeds `MIN_SPACING / speed_mult`.
- [ ] Minimum spacing between consecutive obstacles: `MIN_SPACING_BASE / speed_multiplier` world units (tighter at higher speed).
- [ ] Speed multiplier follows a **piecewise-linear ramp** across 7 brackets (see table below), capping at x3.0 at 180 s.
- [ ] Each difficulty bracket unlocks new obstacle types (cumulative).
- [ ] Lane assignment uses a **weighted random** that penalises the same lane repeating: after 2 consecutive same-lane spawns, that lane's weight drops to 0 for the next spawn.
- [ ] Combo obstacles (Combo Gate, Split Path) are only generated at brackets 4+ (≥ 90 s).
- [ ] Combo obstacle frequency: max 1 combo per 3 obstacles at bracket 4, max 1 per 2 at bracket 5+.
- [ ] Burnout detection range shrinks: `detection_range = BASE_RANGE / (1 + SHRINK_RATE * elapsed_time)`.
- [ ] Obstacles that pass `CLEANUP_DIST` behind the player are destroyed (entity destroyed, components removed).
- [ ] At most **6 obstacle entities** exist at any time (pool ceiling for mobile perf).

## Technical Requirements

### ECS Components (C++ structs)

```cpp
// ── Identifies an entity as an obstacle ──
struct Obstacle {
    float spawn_time;            // seconds since game start
};

// ── What kind of obstacle ──
enum class ObstacleKind : uint8_t {
    ShapeGate,    // requires correct shape
    LaneBlock,    // requires lane change
    LowBar,       // requires jump  (swipe up)
    HighBar,      // requires slide (swipe down)
    ComboGate,    // requires shape + swipe
    SplitPath     // requires shape + correct lane
};

struct ObstacleType {
    ObstacleKind kind;
    ShapeID      required_shape;       // for gates / combos
    SwipeDirection required_swipe;     // for lane/bar/combos
    int          base_points;          // from design doc table
    bool         is_combo;             // true if 2 actions needed
};

// ── World position (shared with rendering) ──
struct Position {
    float x;    // lane position (-1, 0, +1) or pixel
    float y;    // distance along track (scrolls toward player)
};

// ── Lane slot ──
enum class Lane : int8_t {
    Left   = -1,
    Center =  0,
    Right  = +1
};

struct LaneSlot {
    Lane lane;
};

// ── Movement: obstacles scroll toward the player ──
struct Velocity {
    float vy;   // world-units per second (negative = toward player)
};

// ── Singleton: spawn bookkeeping ──
struct SpawnTimer {
    float distance_since_last_spawn; // accumulated dist
    int   obstacles_alive;           // entity count guard
    Lane  last_lane;                 // for anti-repeat rule
    int   same_lane_streak;          // consecutive same-lane count
    int   since_last_combo;          // obstacles since last combo
};

// ── Singleton: difficulty state ──
struct DifficultyState {
    float elapsed_time;          // seconds since game start
    float speed_multiplier;      // 1.0 → 3.0
    int   bracket;               // 0–6 index into difficulty table
    float detection_range;       // current burnout detection range
};
```

### Difficulty Bracket Table (compiled into code)

```
  ┌─────┬──────────┬───────┬──────────────────────────────┬────────────┐
  │ BKT │  TIME    │ SPEED │  UNLOCKED TYPES              │ BURNOUT    │
  ├─────┼──────────┼───────┼──────────────────────────────┼────────────┤
  │  0  │  0–30s   │ x1.0  │ ShapeGate                    │ Very wide  │
  │  1  │ 30–60s   │ x1.3  │ + LaneBlock, LowBar          │ Wide       │
  │  2  │ 60–90s   │ x1.6  │ + HighBar                    │ Moderate   │
  │  3  │ 90–120s  │ x2.0  │ + ComboGate                  │ Tighter    │
  │  4  │ 120–150s │ x2.3  │ + SplitPath                  │ Tight      │
  │  5  │ 150–180s │ x2.6  │ all types, rapid sequences   │ Very tight │
  │  6  │ 180s+    │ x3.0  │ all types, full chaos        │ Razor thin │
  └─────┴──────────┴───────┴──────────────────────────────┴────────────┘
```

### Speed Ramp Formula

```cpp
// Piecewise linear interpolation between bracket endpoints.
// Input:  elapsed seconds
// Output: speed multiplier [1.0, 3.0]
//
// Bracket boundaries (time → speed):
//   { 0, 1.0 }, { 30, 1.3 }, { 60, 1.6 }, { 90, 2.0 },
//   { 120, 2.3 }, { 150, 2.6 }, { 180, 3.0 }
//
// After 180 s: clamped at 3.0
```

### Burnout Window Shrink Formula

```cpp
// detection_range = BASE_RANGE / (1.0 + SHRINK_RATE * elapsed)
//
// At t=0:    12.0 / 1.0 = 12.0  (very generous)
// At t=90:   12.0 / 1.9 ≈  6.3  (moderate)
// At t=180:  12.0 / 2.8 ≈  4.3  (razor thin)
//
// SHRINK_RATE default: 0.01
```

### Systems (function signatures)

```cpp
// Updates DifficultyState: elapsed_time, speed_multiplier, bracket,
// detection_range.  Pure computation, no side effects.
void difficulty_ramp_system(entt::registry& reg, float dt);

// Checks spawn conditions, creates obstacle entities with all
// required components (Obstacle, ObstacleType, Position, LaneSlot,
// Velocity, ObstacleProximity).
// Enforces spacing, lane-repeat, combo-frequency, and pool-cap rules.
void obstacle_spawn_system(entt::registry& reg, float dt);

// Destroys obstacle entities that have scrolled past the player
// beyond CLEANUP_DIST.  Decrements SpawnTimer::obstacles_alive.
void obstacle_cleanup_system(entt::registry& reg, float dt);
```

### Spawn Pipeline (per-frame)

```
  ┌────────────────────────┐
  │ difficulty_ramp_system  │  → updates speed, bracket, det. range
  └───────────┬────────────┘
              │
              ▼
  ┌────────────────────────┐
  │ obstacle_spawn_system   │
  │  ├─ enough spacing?     │  NO → skip
  │  ├─ pool full (>=6)?    │  YES → skip
  │  ├─ pick ObstacleKind   │  (weighted random, bracket-filtered)
  │  ├─ pick lane           │  (anti-repeat weighted)
  │  ├─ if combo: pick 2nd  │  (shape + swipe params)
  │  ├─ create entity       │
  │  │   ├─ Obstacle{}      │
  │  │   ├─ ObstacleType{}  │
  │  │   ├─ Position{}      │
  │  │   ├─ LaneSlot{}      │
  │  │   ├─ Velocity{ -speed }│
  │  │   └─ ObstacleProximity{ det_range }│
  │  └─ update SpawnTimer   │
  └───────────┬────────────┘
              │
              ▼
  ┌────────────────────────┐
  │ obstacle_cleanup_system │  → destroy entities behind player
  └────────────────────────┘
```

### Dependencies

- **Input System (Spec 1)** — `ShapeID` and `SwipeDirection` enums are defined there; re-used here.
- **Burnout Scoring (Spec 2)** — spawned obstacles carry `ObstacleProximity`, which `burnout_fill_system` reads.

### Edge Cases

| Case | Resolution |
|------|------------|
| 3 same-lane spawns in a row | Blocked. After 2 consecutive, that lane weight = 0. |
| Combo at bracket < 3 | Cannot happen; `ComboGate` / `SplitPath` not in the type pool. |
| Pool cap reached (6 alive) | Spawn skipped this frame. Re-checked next frame. |
| Speed multiplier > 3.0 | Clamped to 3.0. |
| Obstacle spawns but player is dead | `obstacle_spawn_system` early-exits if `GameOver` tag exists. |
| Two obstacles overlap spatially | Prevented by minimum spacing rule. If spacing violated by rounding, push new obstacle forward. |
| Combo frequency exceeded | `since_last_combo` counter enforced. If combo would violate 1-in-3 rule, downgrade to simple type. |
| `SplitPath` needs valid lane choice | System picks a lane the player is NOT in, sets `required_shape` randomly. |

## Balancing Parameters

```
  ┌───────────────────────────────────────────────────────────────┐
  │  PARAMETER                  │  DEFAULT  │  NOTES              │
  ├─────────────────────────────┼───────────┼──────────────────────│
  │  SPAWN_HORIZON              │  20.0     │  world units ahead   │
  │  MIN_SPACING_BASE           │  8.0      │  world units         │
  │  CLEANUP_DIST               │ -5.0      │  behind player       │
  │  MAX_ALIVE_OBSTACLES        │  6        │  entity pool cap     │
  │  BASE_DETECTION_RANGE       │  12.0     │  burnout start dist  │
  │  DETECTION_SHRINK_RATE      │  0.01     │  per second          │
  │  SPEED_BRACKET_TIMES[]      │  see tbl  │  7 time points       │
  │  SPEED_BRACKET_VALUES[]     │  see tbl  │  7 speed values      │
  │  SAME_LANE_MAX_STREAK       │  2        │  then force switch   │
  │  COMBO_MIN_GAP_BKT3         │  3        │  obstacles between   │
  │  COMBO_MIN_GAP_BKT5         │  2        │  obstacles between   │
  │  SHAPE_GATE_BASE_PTS        │  200      │                      │
  │  LANE_BLOCK_BASE_PTS        │  100      │                      │
  │  LOW_BAR_BASE_PTS           │  100      │                      │
  │  HIGH_BAR_BASE_PTS          │  100      │                      │
  │  COMBO_GATE_BASE_PTS        │  200      │                      │
  │  SPLIT_PATH_BASE_PTS        │  300      │                      │
  └───────────────────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# CROSS-SPEC INTEGRATION NOTES
# ═══════════════════════════════════════════════════

## System Execution Order (per frame)

```
  ┌─────────────────────────────────────────────────────────────┐
  │                     FRAME TICK (dt)                         │
  │                                                             │
  │  PHASE 1 — INPUT                                           │
  │    1. input_detection_system       (raylib input → touches)  │
  │    2. gesture_recognition_system   (touches → actions)     │
  │                                                             │
  │  PHASE 2 — DIFFICULTY & SPAWNING                           │
  │    3. difficulty_ramp_system       (time → speed/bracket)  │
  │    4. obstacle_spawn_system        (spawn new obstacles)   │
  │                                                             │
  │  PHASE 3 — SCORING                                         │
  │    5. burnout_fill_system          (proximity → meter)     │
  │    6. score_banking_system         (action → bank points)  │
  │    7. chain_tracking_system        (streak → bonus)        │
  │    8. distance_score_system        (time → passive pts)    │
  │                                                             │
  │  PHASE 4 — PHYSICS & CLEANUP                               │
  │    9. (movement systems — not in these specs)              │
  │   10. obstacle_cleanup_system      (destroy passed obs)    │
  │                                                             │
  │  PHASE 5 — RENDER                                          │
  │   11. (render systems — not in these specs)                │
  └─────────────────────────────────────────────────────────────┘
```

## Shared Enum/Type Dependencies

```
  input_system.h         burnout_scoring.h       obstacle_spawning.h
  ──────────────         ─────────────────       ───────────────────
  ShapeID          ────→  (used by banking)  ────→  ObstacleType
  SwipeDirection   ────→  (used by banking)  ────→  ObstacleType
  InputState       ────→  score_banking_system
                          BurnoutMeter
                          ObstacleProximity  ←────  obstacle_spawn_system
                          GameOver           ────→  obstacle_spawn_system
```

## Component Registry Summary

```
  ┌─────────────────────────┬──────────┬─────────────────────────┐
  │  COMPONENT              │ SCOPE    │ DEFINED IN              │
  ├─────────────────────────┼──────────┼─────────────────────────┤
  │  InputState             │ singleton│ Spec 1 — Input          │
  │  TouchEvent             │ embedded │ Spec 1 — Input          │
  │  SwipeGesture           │ embedded │ Spec 1 — Input          │
  │  ShapeButtonTap         │ embedded │ Spec 1 — Input          │
  │  BurnoutMeter           │ singleton│ Spec 2 — Burnout        │
  │  Score                  │ singleton│ Spec 2 — Burnout        │
  │  ChainTracker           │ singleton│ Spec 2 — Burnout        │
  │  ObstacleProximity      │ per-ent  │ Spec 2 — Burnout        │
  │  BurnoutPopup           │ event    │ Spec 2 — Burnout        │
  │  GameOver               │ tag      │ Spec 2 — Burnout        │
  │  Obstacle               │ per-ent  │ Spec 3 — Spawning       │
  │  ObstacleType           │ per-ent  │ Spec 3 — Spawning       │
  │  Position               │ per-ent  │ Spec 3 — Spawning       │
  │  LaneSlot               │ per-ent  │ Spec 3 — Spawning       │
  │  Velocity               │ per-ent  │ Spec 3 — Spawning       │
  │  SpawnTimer             │ singleton│ Spec 3 — Spawning       │
  │  DifficultyState        │ singleton│ Spec 3 — Spawning       │
  └─────────────────────────┴──────────┴─────────────────────────┘
```

---

*End of Feature Specifications — Mid Game Designer, SHAPESHIFTER v1.0*
