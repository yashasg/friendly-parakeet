# SHAPESHIFTER — Feature Specifications
## Mid Game Designer Deliverable · v1.0

> Three implementation-ready specs for the Input System, Burnout Scoring,
> and Obstacle Spawning & Difficulty systems.

> ⚠️ **HISTORICAL — SPEC 2 superseded (issue #239).**
> The "Burnout Scoring System" defined in SPEC 2 below has been **removed
> from the game design**. The burnout meter, burnout zones, burnout
> multiplier, `BurnoutState`/`BurnoutMeter` singletons, and the burnout
> DEAD-zone game-over rule are no longer part of the game. SPEC 2 is
> retained only for historical context.
>
> The current scoring model is rhythm-first: timing-vs-beat grade
> (Perfect/Good/Ok/Bad) × chain. On-beat shape changes are valid play
> regardless of obstacle proximity. The authoritative specs are
> `rhythm-design.md` (design rationale, grade/multiplier table) and
> `rhythm-spec.md` (BPM-derived constants, ECS components, system
> pipeline, collision logic). Failure is owned by the energy bar — see
> `energy-bar.md`. SPEC 1 (Input System) and SPEC 3 (Obstacle Spawning &
> Difficulty) are still partially relevant; cross-check against the
> rhythm specs before relying on numeric values here.
>
> Acceptance/checklist traceability for the unchecked items in this historical
> document lives in `docs/acceptance-traceability.md`.

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
- As a player, I want to **swipe up** so that my character can jump when vertical movement is supported.
- As a player, I want to **swipe down** so that my character can slide when vertical movement is supported.
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
// ── Input source identification ──
enum class InputSource : uint8_t {
    None, Touch, Keyboard
};

// ── Per-frame input state, singleton component ──
struct InputState {
    bool  touch_down = false;       // just pressed this frame
    bool  touch_up   = false;       // just released this frame
    bool  touching   = false;       // held down
    bool  quit_requested = false;

    float start_x = 0, start_y = 0;     // position on touch_down
    float curr_x  = 0, curr_y  = 0;     // latest position
    float end_x   = 0, end_y   = 0;     // position on touch_up
    float duration = 0;                  // seconds held

    InputSource active_source = InputSource::None;
    bool was_focused = true;
};

// ── Event types: semantic player intentions ──
enum class Direction : uint8_t { Left, Right, Up, Down };
enum class InputType : uint8_t { Tap, Swipe };

struct InputEvent {
    InputType type   = InputType::Tap;
    Direction dir    = Direction::Up;   // only meaningful for Swipe
    float     x      = 0.0f;           // virtual-space coordinates
    float     y      = 0.0f;
};

struct ButtonPressEvent {
    entt::entity entity = entt::null;
};

struct GoEvent {
    Direction dir = Direction::Up;
};

struct EventQueue {
    static constexpr int MAX = 8;

    InputEvent       inputs[MAX]  = {};
    int              input_count  = 0;

    ButtonPressEvent presses[MAX] = {};
    int              press_count  = 0;

    GoEvent          goes[MAX]    = {};
    int              go_count     = 0;

    void push_input(InputType t, float px, float py, Direction d = Direction::Up);
    void push_press(entt::entity e);
    void push_go(Direction d);
    void clear();
};
```

### Systems (function signatures)

```cpp
// Reads raylib input (touch + keyboard), populates InputState singleton
// and pushes raw InputEvents into EventQueue.
// Called once per frame in the input phase.
void input_system(entt::registry& reg, float raw_dt);

// Input routing handles raw swipe InputEvents and enqueues GoEvent.
// ButtonPressEvent is emitted by semantic UI/controller paths
// (raygui screen controllers + HUD controls), not raw input routing.
// Runs immediately after input_system.
void wire_input_dispatcher(entt::registry& reg);

// Automated test player: writes EventQueue (push_press, push_go) from
// scripted patterns. Replaces human input when running in test-player mode.
void test_player_system(entt::registry& reg, float dt);
```

### Input → Output Flow

```
  raylib touch/keyboard events
         │
         ▼
  ┌──────────────────────┐
  │ input_system          │  → populates InputState + EventQueue (raw InputEvents)
  └──────────┬───────────┘
             │
             ▼
  ┌──────────────────────┐
  │ input routing         │  → routes swipe InputEvents → GoEvent
  │                      │    (UI/controller paths emit ButtonPressEvent separately)
  └──────────┬───────────┘
             │
             ▼
  ┌──────────────────────┐
  │  gameplay systems     │  → consume ButtonPressEvent + GoEvent from EventQueue
  │  (player_input_sys)  │
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
// Finds the nearest un-cleared obstacle, computes fill, zone & multiplier.
// Writes BurnoutState singleton every frame.
// If fill >= 0.95 and player hasn't acted, emplace GameOver tag.
void burnout_system(entt::registry& reg, float dt);

// On player action: reads current BurnoutState, computes banked points
// with zone multiplier and chain bonus, updates ScoreState, emits
// ScorePopup. Also handles distance scoring (passive pts/second).
void scoring_system(entt::registry& reg, float dt);
```

### Burnout Fill Pipeline

```
  EVERY FRAME:
  ┌──────────────────────────────────────────────┐
  │  1. burnout_system                           │
  │     ├─ find nearest obstacle without         │
  │     │  ScoredTag                             │
  │     ├─ fill = 1 - (dist / detection_range)   │
  │     ├─ clamp fill to [0, 1]                  │
  │     ├─ compute zone + multiplier             │
  │     └─ if fill >= 0.95 → GameOver            │
  └──────────────┬───────────────────────────────┘
                 │
  ON PLAYER ACTION (obstacle cleared):
  ┌──────────────┴───────────────────────────────┐
  │  2. scoring_system                           │
  │     ├─ pts = floor(base * multiplier)        │
  │     ├─ chain bonus from CHAIN_BONUS[]        │
  │     ├─ ScoreState.score += pts + chain_bonus │
  │     ├─ emit ScorePopup                       │
  │     └─ distance scoring: +10 per second      │
  └──────────────────────────────────────────────┘
```

### Dependencies

- **Obstacle Spawning & Difficulty (Spec 3)** — obstacles must exist before burnout_system can compute proximity.
- **Input System (Spec 1)** — `scoring_system` reads `ScoredTag` entities (set by `collision_system`), not the input pipeline directly.

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
  │  CHAIN_BONUS[5]             │  {0,0,   │  indexed by chain     │
  │                             │  50,100, │  count (0..4);        │
  │                             │  200}    │  5+ uses index 4      │
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

> ⚠️ **HISTORICAL / PARTIAL — SPEC 3 stale (issues #420, #446, #475).**
> Major portions of SPEC 3 below describe behavior the runtime no longer
> implements. Specifically these are **not current**:
>
> - **Time-based speed ramp.** The 180-second piecewise-linear `1.0×→3.0×`
>   speed curve and the 7 difficulty brackets are gone. Scroll speed is
>   BPM-derived and constant within a song; difficulty is selected per
>   song (easy / medium / hard) at song-select. See `game.md` "Difficulty
>   Progression" and `rhythm-spec.md` for the authoritative model.
> - **Combo / Split-Path generation at brackets 4+.** No shipped beatmap
>   emits `combo_gate` or `split_path`; `level_designer.py` produces
>   100% `shape_gate` content across all 9 shipped beatmaps. The
>   "combo every 3 obstacles at bracket 4" rule no longer applies.
> - **Burnout detection-range shrink.** The burnout mechanic itself was
>   removed (#239 — see SPEC 2 banner above). `BASE_RANGE / (1 +
>   SHRINK_RATE * elapsed_time)` and any acceptance criterion referencing
>   the burnout window are dead.
>
> Spawn-horizon, cleanup-line, lane anti-repeat, and pool-ceiling
> concepts remain broadly relevant but the numeric values and bracket
> tables below should not be treated as current. Cross-check against
> `rhythm-spec.md` (BPM/window math), `game.md` (difficulty model), and
> `tools/level_designer.py` (actual shipped generation) before relying
> on anything in this section.

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
    ShapeGate,   // requires correct shape
    LaneBlock,   // legacy value kept for backward compat
    ComboGate,   // requires shape + swipe
    SplitPath,   // requires shape + correct lane
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
  │  2  │ 60–90s   │ x1.6  │ ShapeGate                    │ Moderate   │
  │  3  │ 90–120s  │ x2.0  │ + ComboGate                  │ Tighter    │
  │  4  │ 120–150s │ x2.3  │ + SplitPath                  │ Tight      │
  │  5  │ 150–180s │ x2.6  │ active supported types       │ Very tight │
  │  6  │ 180s+    │ x3.0  │ active supported types       │ Razor thin │
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

// Destroys obstacle entities that have scrolled past the camera-Z
// despawn boundary, with a legacy Position.y fallback.
void obstacle_despawn_system(entt::registry& reg, float dt);
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
  │ obstacle_despawn_system │  → destroy entities behind player
  └────────────────────────┘
```

### Dependencies

- **Input System (Spec 1)** — `ShapeID` and `SwipeDirection` enums are defined there; re-used here.
- **Burnout Scoring (Spec 2)** — spawned obstacles are queried by `burnout_system` for proximity.

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
  │    1. input_system                 (raylib input → state)   │
  │                                                             │
  │  PHASE 2 — DIFFICULTY & SPAWNING                           │
  │    2. difficulty_system            (time → speed/bracket)   │
  │    3. obstacle_spawn_system        (spawn new obstacles)    │
  │                                                             │
  │  PHASE 3 — SCORING                                         │
  │    4. burnout_system               (proximity → meter)      │
  │    5. scoring_system               (action → bank + chain)  │
  │                                                             │
  │  PHASE 4 — PLAYER & PHYSICS                                │
  │    6. player_input_system          (EventQueue → player)    │
  │    7. (movement systems — not in these specs)              │
  │    8. obstacle_despawn_system      (destroy passed obs)    │
  │                                                             │
  │  PHASE 5 — RENDER                                          │
  │   11. (render systems — not in these specs)                │
  └─────────────────────────────────────────────────────────────┘
```

## Shared Enum/Type Dependencies

```
  input.h                burnout/scoring.h       obstacle_spawning.h
  ──────────────         ─────────────────       ───────────────────
  InputState       ────→  player_input_system
  EventQueue       ────→  player_input_system
                          BurnoutState     ←────  burnout_system
                          ScoreState       ←────  scoring_system
                          GameOver         ────→  obstacle_spawn_system
```

## Component Registry Summary

```
  ┌─────────────────────────┬──────────┬─────────────────────────┐
  │  COMPONENT              │ SCOPE    │ DEFINED IN              │
  ├─────────────────────────┼──────────┼─────────────────────────┤
  │  InputState             │ singleton│ Spec 1 — Input          │
  │  EventQueue            │ singleton│ Spec 1 — Input          │
  │  BurnoutState           │ singleton│ Spec 2 — Burnout        │
  │  ScoreState             │ singleton│ Spec 2 — Burnout        │
  │  ScorePopup             │ per-ent  │ Spec 2 — Burnout        │
  │  GameOver               │ tag      │ Spec 2 — Burnout        │
  │  Obstacle               │ per-ent  │ Spec 3 — Spawning       │
  │  ObstacleType           │ per-ent  │ Spec 3 — Spawning       │
  │  ScoredTag              │ tag      │ Spec 3 — Spawning       │
  │  Position               │ per-ent  │ Spec 3 — Spawning       │
  │  LaneSlot               │ per-ent  │ Spec 3 — Spawning       │
  │  Velocity               │ per-ent  │ Spec 3 — Spawning       │
  │  SpawnTimer             │ singleton│ Spec 3 — Spawning       │
  │  DifficultyState        │ singleton│ Spec 3 — Spawning       │
  └─────────────────────────┴──────────┴─────────────────────────┘
```

---

*End of Feature Specifications — Mid Game Designer, SHAPESHIFTER v1.0*
