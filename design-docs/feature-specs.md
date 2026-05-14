# SHAPESHIFTER — Feature Specifications
## Mid Game Designer Deliverable · v1.0

> Three implementation-ready specs for the Input System, Rhythm Scoring,
> and Beatmap Scheduling systems.
>
> The removed Burnout risk/reward model and the old random/time-ramp obstacle
> spawner are not current game design. Current scoring is rhythm-first:
> timing-vs-beat grade (Perfect/Good/Ok/Bad) × chain, with survival owned by
> the energy bar. Current obstacle scheduling is authored beatmap playback
> against song time, not elapsed-time difficulty brackets.
>
> See `rhythm-design.md` for design rationale, `rhythm-spec.md` for the
> authoritative rhythm ECS pipeline, and `energy-bar.md` for survival rules.
> Acceptance/checklist traceability for remaining unchecked items lives in
> `docs/acceptance-traceability.md`.

```
  ┌─────────────────────────────────────────────────┐
  │  SPEC 1: Input System ..................... p.1  │
  │  SPEC 2: Rhythm Scoring System ........... p.2  │
  │  SPEC 3: Beatmap Scheduling .............. p.3  │
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
enum class InputSource : uint8_t { None, Mouse, Touch };

// ── Per-touch tracking slot (multi-touch support) ──
struct TouchSlot {
    static constexpr int InvalidId = -1;

    int   id = InvalidId;
    bool  active = false;
    bool  started_in_button_zone = false;
    float start_x = 0.0f, start_y = 0.0f;
    float curr_x  = 0.0f, curr_y  = 0.0f;
    float duration = 0.0f;
};

// ── Per-frame input state, singleton component ──
// Tracks touch/mouse hardware state. Downstream systems should read
// semantic events (GoEvent / ButtonPressEvent) off the EnTT dispatcher,
// not this struct — except for quit_requested.
struct InputState {
    static constexpr int MaxTrackedTouches = 2;

    float start_x  = 0.0f, start_y = 0.0f;  // position on touch_down
    float curr_x   = 0.0f, curr_y  = 0.0f;  // latest position
    float end_x    = 0.0f, end_y   = 0.0f;  // position on touch_up
    float duration = 0.0f;                  // seconds held

    InputSource active_source = InputSource::None;
    bool  touch_down     = false;           // just pressed this frame
    bool  touch_up       = false;           // just released this frame
    bool  click          = false;           // tap recognised this frame
    bool  touching       = false;           // held down
    bool  quit_requested = false;
    bool  was_focused = true;
    bool  gestures_configured    = false;
    bool  suppress_mouse_release = false;
    bool  button_touch_up        = false;
    float button_end_x = 0.0f, button_end_y = 0.0f;
    TouchSlot touch_slots[MaxTrackedTouches] = {};
};

// ── Event types: semantic player intentions ──
//
// ButtonPressEvent carries semantic value data encoded at source (#273).
// Consumers act on kind/shape/menu_action — never on a live entity handle,
// which would be a lifetime hazard if the button entity were destroyed
// between event production and consumption.

enum class Direction : uint8_t { Left, Right, Up, Down };  // defined in input.h

enum class MenuActionKind : uint8_t {
    Confirm       = 0,
    Restart       = 1,
    GoLevelSelect = 2,
    GoMainMenu    = 3,
    Exit          = 4,
    SelectLevel   = 5,
    SelectDiff    = 6,
};

enum class ButtonPressKind : uint8_t {
    Shape,  // shape button pressed — use .shape
    Menu,   // menu button pressed  — use .menu_action / .menu_index
};

struct ButtonPressEvent {
    ButtonPressKind kind        = ButtonPressKind::Shape;
    Shape           shape       = Shape::Circle;            // valid when kind == Shape
    MenuActionKind  menu_action = MenuActionKind::Confirm;  // valid when kind == Menu
    uint8_t         menu_index  = 0;                        // valid when kind == Menu
};

struct GoEvent {
    Direction dir = Direction::Up;
};
```

Events ride on the registry-scoped `entt::dispatcher` (stored in
`registry.ctx()`). Producers call `disp.enqueue<EventT>(...)`; the
fixed-step drain in `game_state_system` calls `disp.update<EventT>()`,
which fans events out to all connected sinks.

### Systems (function signatures)

```cpp
// Reads raylib input (touch + keyboard), populates InputState singleton,
// and enqueues semantic events on the registry's entt::dispatcher
// (GoEvent for swipes/arrow keys, ButtonPressEvent for shape/menu keys).
// Called once per frame in the input phase.
void input_system(entt::registry& reg, float raw_dt);

// Connects sinks for GoEvent and ButtonPressEvent on the registry's
// dispatcher. Wires game_state, level_select, and player_input handlers.
// Called once at startup (and on dispatcher swap).
void wire_input_dispatcher(entt::registry& reg);

// raygui screen controllers (e.g. gameplay_hud_screen_controller) enqueue
// ButtonPressEvent on UI hits — they are the dominant ButtonPressEvent
// producer alongside input_system's keyboard fallbacks.
void gameplay_hud_process_button_input(entt::registry& reg);

// Automated test player: enqueues GoEvent / ButtonPressEvent on the
// dispatcher from scripted patterns. Replaces human input when running
// in test-player mode.
void test_player_system(entt::registry& reg, float dt);

// Drain: inside game_state_system's fixed-step tick, call
//   disp.update<ButtonPressEvent>();
//   disp.update<GoEvent>();
// to fan queued events out to all connected handlers.
void game_state_system(entt::registry& reg, float dt);
```

### Input → Output Flow

```
  raylib touch/keyboard events
         │
         ▼
  ┌──────────────────────┐
  │ input_system          │  → populates InputState
  │                      │  → disp.enqueue<GoEvent>         (swipe / arrows)
  │                      │  → disp.enqueue<ButtonPressEvent> (shape keys)
  └──────────┬───────────┘
             │
             ▼
  ┌──────────────────────┐
  │ HUD / test player     │  → disp.enqueue<ButtonPressEvent>
  │                      │    (raygui screen controllers,
  │                      │     test_player_system scripted runs)
  └──────────┬───────────┘
             │
             ▼
  ┌──────────────────────┐
  │ entt::dispatcher      │  → game_state_system drains via
  │  sinks (fixed step)  │    disp.update<ButtonPressEvent>()
  │                      │    disp.update<GoEvent>()
  │                      │  → fans out to game_state, level_select,
  │                      │    and player_input handlers
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
# SPEC 2 — RHYTHM SCORING SYSTEM
# ═══════════════════════════════════════════════════

## Overview

Scoring rewards shape changes and obstacle clears that align with the song
beat. The player earns a timing grade when the active shape window intersects
an obstacle's calibrated arrival time. Points are based on obstacle base
points, timing multiplier, and chain multiplier. Misses and Bad hits affect
the energy bar; energy reaching zero ends the run.

```
  TIMING LADDER

  beat time
      │
      ├── ±50ms  → PERFECT
      ├── ±100ms → GOOD
      ├── ±150ms → OK
      └── active window outside OK → BAD

  score = base_points × timing_multiplier × chain_multiplier
```

## User Stories

- As a player, I want **clear timing grades** so I understand whether I hit the beat.
- As a player, I want **higher rewards for tighter timing** so mastery feels valuable.
- As a player, I want **chain bonuses** for consecutive successful clears so streaks feel rewarding.
- As a player, I want **misses and late/loose hits to affect energy** so survival stays readable.
- As a player, I want **passive distance points** so I always feel like I'm earning something.

## Acceptance Criteria

- [ ] PERFECT is awarded for hits within ±50 ms of calibrated beat arrival.
- [ ] GOOD is awarded for hits within ±100 ms.
- [ ] OK is awarded for hits within ±150 ms.
- [ ] BAD is awarded for scored hits outside OK but still inside the active shape window.
- [ ] Successful clears increment `ScoreState::chain_count`; misses reset it.
- [ ] Chain multiplier increases by `CHAIN_MULT_STEP` per chain step and caps at the rhythm-spec limit.
- [ ] Passive distance score accumulates at `PTS_PER_SECOND` while playback is active.
- [ ] Song results track Perfect / Good / OK / Bad / Miss / Max Chain / Energy for terminal screens.
- [ ] Energy effects are owned by the scoring/energy pipeline: PERFECT/GOOD/OK recover, BAD and MISS drain.

## Technical Requirements

### ECS Components (C++ structs)

```cpp
enum class TimingTier : uint8_t {
    Bad,
    Ok,
    Good,
    Perfect,
};

struct TimingGrade {
    TimingTier tier      = TimingTier::Bad;
    float      precision = 0.0f;  // 0.0 = edge, 1.0 = dead center
};

struct ScoreState {
    int32_t score = 0;
    int32_t displayed_score = 0;
    int32_t high_score = 0;
    int32_t chain_count = 0;
    float distance_traveled = 0.0f;
    float passive_score_remainder = 0.0f;
};

struct ScorePopup {
    int32_t value = 0;
    bool has_timing_tier = false;
    TimingTier timing_tier = TimingTier::Ok;
    float remaining = 0.0f;
    float max_time = 0.0f;
};
```

### Systems (function signatures)

```cpp
// Processes ScoredTag/MissTag obstacles, applies timing/chain scoring,
// queues energy effects, emits score popups, and accrues passive score.
void scoring_system(entt::registry& reg, float dt);

// Applies queued score popup lifetime/display data.
void popup_feedback_system(entt::registry& reg, float dt);
```

### Scoring Pipeline

```
  collision / miss systems
          │
          ├─ ScoredTag + TimingGrade → scoring_system hit pass
          ├─ ScoredTag + MissTag     → scoring_system miss pass
          │
          ▼
  ScoreState + SongResults + EnergyEffectQueue + ScorePopup
```

### Dependencies

- **Beatmap Scheduling (Spec 3)** — scheduled obstacles carry base points and calibrated beat arrival data.
- **Input System (Spec 1)** — player shape changes define the active timing window; scoring reads obstacle tags emitted by collision/miss systems.
- **Energy Bar** — scoring queues recovery/drain events; `energy_system` owns final survival state.

### Edge Cases

| Case | Resolution |
|------|------------|
| Hit exactly on a timing boundary | Use the tighter tier if `abs(offset)` is within that tier's inclusive threshold. |
| Obstacle is `NonScorableTag` | No score popup, no chain contribution, no grade result. |
| Obstacle passes unscored | Miss path drains energy, increments miss count, and resets chain. |
| Multiple scored entities in one tick | Collect first, then remove structural tags after iteration for EnTT safety. |
| Distance scoring during pause/menu | `dt` is 0 while paused; no points accumulate. |

## Balancing Parameters

```
  ┌──────────────────────────────────────────────────────────────┐
  │  PARAMETER                │  DEFAULT  │  NOTES               │
  ├───────────────────────────┼───────────┼───────────────────────│
  │  WINDOW_PERFECT           │  0.050 s  │  ± timing threshold   │
  │  WINDOW_GOOD              │  0.100 s  │  ± timing threshold   │
  │  WINDOW_OK                │  0.150 s  │  ± timing threshold   │
  │  CHAIN_MULT_STEP          │  0.05     │  per chain step       │
  │  CHAIN_MULT_CAP           │  2.0x     │  current cap          │
  │  PTS_PER_SECOND           │  10       │  passive income       │
  └──────────────────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# SPEC 3 — BEATMAP SCHEDULING
# ═══════════════════════════════════════════════════

## Overview

This system materializes authored beatmap entries as obstacle entities.
Difficulty is selected at song-select (`easy`, `medium`, `hard`) and encoded
in the chosen beat array. Runtime scheduling compares song time against each
entry's calibrated arrival time and spawn lead time. Scroll speed is derived
from BPM/lead-time math and stays constant within a song.

```
  AUTHORED BEATMAP PLAYBACK

  BeatMap.difficulties[difficulty].beats
          │
          ├─ onset_marker → metadata only; scheduler skips spawn
          └─ shape_gate / split_path
                    │
                    ▼
  calibrated_arrival_time = authored_beat_time + audio_offset_sec
  spawn_time = calibrated_arrival_time - lead_time
                    │
                    ▼
  spawn_rhythm_obstacle(... BeatInfo{beat_index, arrival, spawn})
```

## User Stories

- As a player, I want **obstacles to land on musically meaningful beats** so the level feels rhythm-driven.
- As a player, I want **difficulty to be selected before the song** so the challenge is predictable.
- As a level designer, I want **authored beat arrays** so obstacles can follow song structure and onset analysis.
- As a player using calibration, I want **obstacle timing to match my audio offset**.

## Acceptance Criteria

- [ ] The selected `BeatMap` difficulty is the only source of scheduled obstacles for a song.
- [ ] `shape_gate` entries spawn with required shape, lane, base points, and calibrated `BeatInfo`.
- [ ] `onset_marker` entries never spawn runtime obstacles and are treated as authoring metadata.
- [ ] Explicit `time_sec` entries override beat-index lookup; otherwise beat times use `beat_times[beat_index]` when present, then `offset + beat_index * beat_period`.
- [ ] Positive `audio_offset_ms` delays obstacle arrival; negative values advance it.
- [ ] Late spawns compensate overshoot so the obstacle still reaches `PLAYER_Y` at the calibrated arrival time, clamped at the player line.
- [ ] Obstacles that pass the camera-Z despawn boundary are destroyed.
- [ ] Invalid lanes are reported through the existing logging path and default to center where runtime supports that fallback.

## Technical Requirements

### ECS Components (C++ structs)

```cpp
struct Obstacle {
    int16_t base_points = 200;
};

enum class ObstacleKind : uint8_t {
    ShapeGate,
    LaneBlock,   // legacy component fixture only; active beatmaps/factories reject
    ComboGate,   // legacy component fixture only; active beatmaps/factories reject
    SplitPath,
    OnsetMarker,
};

struct RequiredShape {
    Shape shape = Shape::Circle;
};

struct RequiredLane {
    int8_t lane = 0;
};

struct BlockedLanes {
    uint8_t mask = 0;
};

struct BeatInfo {
    int beat_index = 0;
    float arrival_time = 0.0f;
    float spawn_time = 0.0f;
};

// Cold asset singleton. Attached to the BeatMapTag entity created by
// create_beat_map_entity(); populated in setup_play_session() and
// read-only during gameplay. Copy operations are deleted so accidental
// duplication of the heap-allocated beat arrays is a compile-time error;
// use std::move() to transfer ownership.
struct BeatMap {
    std::string song_id;
    std::string title;
    std::string song_path;
    float       bpm        = 120.0f;
    float       offset     = 0.0f;
    int         lead_beats = 4;
    float       duration   = 180.0f;
    std::string difficulty;
    std::vector<float>     beat_times;
    std::vector<BeatEntry> beats;

    BeatMap()                          = default;
    BeatMap(const BeatMap&)            = delete;
    BeatMap& operator=(const BeatMap&) = delete;
    BeatMap(BeatMap&&)                 = default;
    BeatMap& operator=(BeatMap&&)      = default;
};
```

### Beat Time Resolution

```cpp
float beat_time = song.offset + entry.beat_index * song.beat_period;
if (!entry.has_time_sec && beat_index_in_range(map.beat_times, entry.beat_index)) {
    beat_time = map.beat_times[entry.beat_index];
}
if (entry.has_time_sec) {
    beat_time = entry.time_sec;
}

float calibrated_arrival_time = beat_time + audio_offset_seconds(settings);
float spawn_time = calibrated_arrival_time - song.lead_time;
```

### Systems (function signatures)

```cpp
// Spawns authored beatmap entries whose spawn_time has arrived.
// Skips OnsetMarker entries.
void beat_scheduler_system(entt::registry& reg, float dt);

// Destroys obstacle entities that have scrolled past the camera-Z
// despawn boundary, with a legacy Position.y fallback.
void obstacle_despawn_system(entt::registry& reg, float dt);
```

### Spawn Pipeline (per-frame)

```
  ┌────────────────────────┐
  │ song_playback_system    │  → song_time, current beat, finished state
  └───────────┬────────────┘
              ▼
  ┌────────────────────────┐
  │ beat_scheduler_system   │
  │  ├─ next entry ready?   │  NO → stop
  │  ├─ OnsetMarker?       │  YES → advance without spawn
  │  ├─ resolve beat time   │
  │  ├─ apply calibration   │
  │  ├─ compensate overshoot│
  │  └─ spawn obstacle      │
  └───────────┬────────────┘
              ▼
  ┌────────────────────────┐
  │ obstacle_despawn_system │  → destroy entities past despawn boundary
  └────────────────────────┘
```

### Dependencies

- **Input System (Spec 1)** — `ShapeID` and `SwipeDirection` enums are defined there; re-used here.
- **Rhythm Scoring (Spec 2)** — spawned obstacles provide `BeatInfo`, `Obstacle`, and required-action components to collision/scoring.
- **Settings** — audio calibration shifts beat arrival and spawn timing.

### Edge Cases

| Case | Resolution |
|------|------------|
| Entry kind is `OnsetMarker` | Advance `next_spawn_idx`; no entity is created. |
| Beat index lacks `beat_times` entry | Fall back to `offset + beat_index * beat_period`. |
| Entry has explicit `time_sec` | Use it as the authoritative beat time. |
| Scheduler wakes up after spawn time | Place obstacle partway down the runway using overshoot compensation. |
| Overshoot would spawn past player line | Clamp start position at `PLAYER_Y` and adjust stored spawn time. |
| Song is not playing | Scheduler early-outs without mutating spawn index. |

## Balancing Parameters

```
  ┌───────────────────────────────────────────────────────────────┐
  │  PARAMETER                  │  DEFAULT  │  NOTES              │
  ├─────────────────────────────┼───────────┼──────────────────────│
  │  LEAD_BEATS                  │ beatmap   │ spawn lead distance │
  │  BEAT_PERIOD                 │ 60 / BPM  │ per song            │
  │  SCROLL_SPEED                │ derived   │ BPM + lead-time     │
  │  AUDIO_OFFSET_MS            │ settings  │ calibration         │
  │  SHAPE_GATE_BASE_PTS        │ 200       │ scoring base        │
  │  COMBO_GATE_BASE_PTS        │ 200       │ supported kind      │
  │  SPLIT_PATH_BASE_PTS        │ 300       │ supported kind      │
  └───────────────────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# CROSS-SPEC INTEGRATION NOTES
# ═══════════════════════════════════════════════════

## System Execution Order

Per-frame systems outside the fixed timestep:

```
compute_screen_transform -> input_system -> gameplay_hud_process_button_input ->
test_player_system -> fixed timestep loop -> game_camera_system ->
ui_camera_system -> game_render_system -> ui_render_system ->
audio_system -> haptic_system
```

Each fixed timestep runs `tick_fixed_systems`:

```
game_state_system -> song_playback_system -> tick_playing_systems ->
obstacle_despawn_system -> popup_feedback_system -> popup_display_system ->
energy_system -> energy_bar_system -> particle_system
```

`tick_playing_systems` is a `GamePhase::Playing`-gated bundle:

```
beat_log_system -> beat_scheduler_system -> shape_window_activation_system ->
player_movement_system -> scroll_system -> motion_system -> collision_system ->
shape_window_system -> miss_detection_system -> scoring_system
```

## Shared Enum/Type Dependencies

```
  input.h                rhythm/scoring.h        beatmap scheduling
  ──────────────         ─────────────────       ───────────────────
  InputState       ────→  player_input_system
  ButtonPressEvent ────→  player_input_system  (via entt::dispatcher sink)
  GoEvent          ────→  player_input_system  (via entt::dispatcher sink)
                          TimingGrade      ←────  collision_system
                          ScoreState       ←────  scoring_system
                          BeatInfo         ←────  beat_scheduler_system
```

## Component Registry Summary

```
  ┌─────────────────────────┬──────────┬─────────────────────────┐
  │  COMPONENT              │ SCOPE    │ DEFINED IN              │
  ├─────────────────────────┼──────────┼─────────────────────────┤
  │  InputState             │ singleton│ Spec 1 — Input          │
  │  ButtonPressEvent       │ event    │ Spec 1 — Input          │
  │  GoEvent                │ event    │ Spec 1 — Input          │
  │  ScoreState             │ singleton│ Spec 2 — Rhythm Scoring │
  │  ScorePopup             │ per-ent  │ Spec 2 — Rhythm Scoring │
  │  TimingGrade            │ per-ent  │ Spec 2 — Rhythm Scoring │
  │  Obstacle               │ per-ent  │ Spec 3 — Scheduling     │
  │  BeatInfo               │ per-ent  │ Spec 3 — Scheduling     │
  │  RequiredShape          │ per-ent  │ Spec 3 — Scheduling     │
  │  RequiredLane           │ per-ent  │ Spec 3 — Scheduling     │
  │  BlockedLanes           │ per-ent  │ Spec 3 — Scheduling     │
  │  ScoredTag              │ tag      │ Spec 2/3 bridge         │
  │  MissTag                │ tag      │ Spec 2/3 bridge         │
  └─────────────────────────┴──────────┴─────────────────────────┘
```

---

*End of Feature Specifications — Mid Game Designer, SHAPESHIFTER v1.0*
