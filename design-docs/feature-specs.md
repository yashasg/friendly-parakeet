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

The Input System translates raw touch / mouse / keyboard events into game actions (lane changes, jump, slide, shape shifts). It owns zone detection and gesture recognition, then emits typed input events through the EnTT dispatcher so consumer systems pick them up in raylib's frame order.

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
- As a player, I want to **swipe AND tap nearly simultaneously** so that quick shape changes during movement both register in the same frame.
- As a player, I want **accidental micro-touches** (jitter) to be ignored so I don't misfire.

## Acceptance Criteria

- [ ] Touches that begin at `y >= 0.80 * screen_height` are routed to button detection; all others to swipe detection.
- [ ] A swipe is recognised when `distance >= MIN_SWIPE_DIST` (50 dp) AND `duration <= MAX_SWIPE_TIME` (300 ms).
- [ ] Swipe direction is the dominant-axis direction of the delta vector, using 45° quadrant boundaries.
- [ ] A swipe that does not meet distance OR time thresholds is discarded (no action).
- [ ] A button tap is recognised on touch-up within the same button's hit-rect.
- [ ] Tapping the **already-active** shape button is a no-op (no event emitted).
- [ ] Multi-touch: at most **2 simultaneous touches** are tracked (one per zone). Third+ fingers are ignored.
- [ ] On raylib touch events, finger positions are converted from screen coords to logical coords before processing.

## Technical Requirements

### ECS Components (C++ structs)

```cpp
// ── Input source ctx-tag mutex (former enum InputSource) ──
// Per-source ctx tables replace the former 3-state enum on
// InputState::active_source (issues #1202 / #1204):
//   • presence of InputSourceMouse ⇔ Mouse owns the gesture
//   • presence of InputSourceTouch ⇔ Touch owns the gesture
//   • absence of both              ⇔ None
// The mutex is maintained inline at the two set sites in input_system
// (insert one tag + erase the sibling) and via the shared
// `clear_input_source` helper which drops both tags.
// Declared in `app/tags/tags.h` (canonical tags single-header per
// issue #1645).
struct InputSourceMouse {};
struct InputSourceTouch {};

// ── Active touch slot — one row per currently-tracked finger (#1612) ──
// Per Fabian Principle 3, the former fixed-size `TouchSlot
// touch_slots[MaxTrackedTouches]` array column on `InputState` + its
// `id == InvalidId = -1` NULL column was normalized into a row table:
// presence in `view<ActiveTouchSlot>()` IS slot activity, and the
// `MaxTrackedTouches = 2` policy cap survives as a runtime allocation
// guard at the create site in `input_system`.
struct ActiveTouchSlot {
    int   id;
    bool  started_in_button_zone;
    float start_x, start_y;
    float curr_x,  curr_y;
    float duration;
};

// ── Per-frame input state, singleton component ──
// Tracks touch/mouse hardware state. Downstream systems should read
// semantic events (Go*Event / ShapePress*Event / Menu*Event) off the
// EnTT dispatcher, not this struct — except for quit_requested.
struct InputState {
    static constexpr int MaxTrackedTouches = 2;

    float start_x  = 0.0f, start_y = 0.0f;  // position on touch_down
    float curr_x   = 0.0f, curr_y  = 0.0f;  // latest position
    float end_x    = 0.0f, end_y   = 0.0f;  // position on touch_up
    float duration = 0.0f;                  // seconds held

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
};

// ── Event types: semantic player intentions ──
//
// Per Fabian's existential processing (#1202/#1204/#1277/#1278/#1279),
// each former `ButtonPressKind` × shape, each former `MenuActionKind`
// value, and each former `Direction` value is now its own event type.
// Listeners subscribe to the specific event they handle — the type IS
// the choice. Events carry concrete values (or nothing) — never a live
// entity handle, which would be a lifetime hazard if the button entity
// were destroyed between event production and consumption.

// Shape presses are zero-column tag-events.
struct ShapePressCircleEvent   {};
struct ShapePressSquareEvent   {};
struct ShapePressTriangleEvent {};

// Per-action menu events.
struct MenuConfirmEvent       {};
struct MenuRestartEvent       {};
struct MenuGoLevelSelectEvent {};
struct MenuGoMainMenuEvent    {};
struct MenuSelectLevelEvent { uint8_t index = 0; };
struct MenuSelectDiffEvent  { uint8_t index = 0; };

// Per-direction navigation events (post-#1279).
struct GoUpEvent    {};
struct GoDownEvent  {};
struct GoLeftEvent  {};
struct GoRightEvent {};
```

Events ride on the registry-scoped `entt::dispatcher` (stored in
`registry.ctx()`). Producers call `disp.enqueue<EventT>(...)`; the
fixed-step drain in `game_state_system` calls `disp.update<EventT>()`,
which fans events out to all connected sinks.

### Systems (function signatures)

```cpp
// Reads raylib input (touch + keyboard), populates InputState singleton,
// and enqueues semantic events on the registry's entt::dispatcher
// (Go*Event for swipes/arrow keys, ShapePress*Event / Menu*Event for
// shape/menu keys). Called once per frame in the input phase.
void input_system(entt::registry& reg, float raw_dt);

// Connects sinks for the per-direction Go*Event types and the
// per-action ShapePress*Event / Menu*Event types on the registry's
// dispatcher. Wires game_state, level_select, and player_input handlers.
// Called once at startup (and on dispatcher swap).
void wire_input_dispatcher(entt::registry& reg);

// Entity-driven UI dispatch (issue #1287, completed in #1297): each
// migrated screen — including the gameplay HUD — relies on
// `ui_update_system` to hit-test `UiPosition`/`UiBounds`/`OnPress`
// entities and dispatch the matching action. The screen-spawner
// functions in `app/systems/generated/<screen>_screen.cpp` (codegen output)
// emplace those entities at phase entry. Gameplay HUD lane buttons
// (Circle / Square / Triangle) emit `ShapePress*Event`s through this
// same system; the Pause button enqueues a `NextPhasePausedTag` phase
// transition.
void ui_update_system(entt::registry& reg);

// Automated test player: enqueues Go*Event / ShapePress*Event on the
// dispatcher from scripted patterns. Replaces human input when running
// in test-player mode.
void test_player_system(entt::registry& reg, float dt);

// Drain: inside game_state_system's fixed-step tick, call
//   disp.update<ShapePressCircleEvent>();    // (+ Square, Triangle)
//   disp.update<MenuConfirmEvent>();         // (+ other menu events)
//   disp.update<GoUpEvent>();                // (+ Down, Left, Right)
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
  │                      │  → disp.enqueue<Go{Up,Down,Left,Right}Event> (swipe / arrows)
  │                      │  → disp.enqueue<ShapePress*Event>            (shape keys)
  │                      │  → disp.enqueue<Menu*Event>                   (menu keys)
  └──────────┬───────────┘
             │
             ▼
  ┌──────────────────────┐
  │ HUD / test player     │  → disp.enqueue<ShapePress*Event>
  │                      │  → disp.enqueue<Menu*Event>
  │                      │    (raygui screen controllers,
  │                      │     test_player_system scripted runs)
  └──────────┬───────────┘
             │
             ▼
  ┌──────────────────────┐
  │ entt::dispatcher      │  → game_state_system drains via
  │  sinks (fixed step)  │    disp.update<ShapePress*Event>()
  │                      │    disp.update<Menu*Event>()
  │                      │    disp.update<Go{Up,Down,Left,Right}Event>()
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
| Rapid double-tap on same button | Each tap emits its own event; consumer systems decide whether to re-act. |
| Tap on already-selected shape | No event emitted. |
| Simultaneous swipe + tap (combo) | Both events are emitted on the dispatcher in raylib's gesture order. |

## Balancing Parameters

```
  ┌───────────────────────────────────────────────────────────┐
  │  PARAMETER              │  DEFAULT   │  NOTES             │
  ├─────────────────────────┼────────────┼─────────────────────│
  │  ZONE_SPLIT_Y           │  0.80      │  fraction of screen │
  │  MIN_SWIPE_DIST_DP      │  50        │  density-indep px   │
  │  MAX_SWIPE_TIME_SEC     │  0.300     │  seconds            │
  │  BUTTON_HIT_PADDING_DP  │  12        │  expand hit-rect    │
  │  MAX_SIMULTANEOUS_TOUCH │  2         │  fingers tracked    │
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
// The former `enum class TimingTier { Bad, Ok, Good, Perfect }` was
// eradicated per issue #1202/#1204 (Fabian existential processing).
// The tier discriminator now lives as one of four zero-column tags
// (TimingPerfectTag / TimingGoodTag / TimingOkTag / TimingBadTag) from
// `app/tags/tags.h`, emplaced on the obstacle/popup entity by
// `emplace_timing_tier_tag_for_delta_abs()` in `app/util/rhythm_math.h`.

struct TimingGrade {
    float precision = 0.0f;  // 0.0 = edge, 1.0 = dead center
};

struct ScoreState {
    int32_t score = 0;
    int32_t chain_count = 0;
    float passive_score_remainder = 0.0f;
};

// ScoreDisplay holds the HUD's smoothly-animated readout.
struct ScoreDisplay {
    int32_t displayed = 0;
};

// ScorePopup carries only the value + lifetime. The tier discriminator is a
// per-tier tag (Timing*Tag) on the popup entity itself; static text/color are
// baked into a sibling `PopupDisplay` row at spawn time.
struct ScorePopup {
    int32_t value     = 0;
    float   remaining = 0.0f;
    float   max_time  = 0.0f;
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
  spawn_<kind>_rhythm(... BeatInfo{beat_index, arrival, spawn})
   ├─ spawn_shape_gate_rhythm  (kind = shape_gate)
   └─ spawn_split_path_rhythm  (kind = split_path)
  (defined in app/entities/obstacle_entity.h; the scheduler
   skips spawn for onset_marker per the metadata-only branch above)
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

// The former `enum class ObstacleKind { ShapeGate, SplitPath, OnsetMarker }`
// was eradicated per issue #1202/#1204. Archetype identity now lives as a
// zero-column tag in `app/tags/tags.h`:
//   ShapeGateTag   — must match shape; lane stored as raw `int8_t` component.
//   SplitPathTag   — shape + dodge lane; raw `int8_t` component.
//   OnsetMarkerTag — visual beat marker; non-blocking, non-scoring.
// Consumers filter by view (`view<ShapeGateTag, …>` vs `view<SplitPathTag, …>`);
// no `switch (kind)` dispatch remains in `app/`.

// `RequiredShape { Shape shape }` was unwrapped to per-shape tags
// (`RequiredShapeCircleTag` / …`SquareTag` / …`TriangleTag` / …`HexagonTag`)
// on the obstacle entity. Read via `current_required_shape(reg, e)` from
// `app/util/shape_tag.h`.

// Lane index is a raw int8_t component on the obstacle entity (issue #1198).
// Semantics depend on the archetype tag — ShapeGateTag = positional lane,
// SplitPathTag = required dodge lane.

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
    std::vector<float>          beat_times;
    // Fourteen per-(kind, shape, time-source) vectors — see rhythm-spec.md
    // and `app/components/beat_map.h`. The former single `beats` vector
    // was decomposed by issues #1198 / #1202 / #1204 / #1533.
    std::vector<BeatEntry>      shape_gate_circle_beats;
    std::vector<BeatEntry>      shape_gate_square_beats;
    std::vector<BeatEntry>      shape_gate_triangle_beats;
    std::vector<BeatEntry>      split_path_circle_beats;
    std::vector<BeatEntry>      split_path_square_beats;
    std::vector<BeatEntry>      split_path_triangle_beats;
    std::vector<BeatEntry>      onset_marker_beats;
    std::vector<BeatEntryTimed> shape_gate_circle_beats_timed;
    std::vector<BeatEntryTimed> shape_gate_square_beats_timed;
    std::vector<BeatEntryTimed> shape_gate_triangle_beats_timed;
    std::vector<BeatEntryTimed> split_path_circle_beats_timed;
    std::vector<BeatEntryTimed> split_path_square_beats_timed;
    std::vector<BeatEntryTimed> split_path_triangle_beats_timed;
    std::vector<BeatEntryTimed> onset_marker_beats_timed;

    BeatMap()                          = default;
    BeatMap(const BeatMap&)            = delete;
    BeatMap& operator=(const BeatMap&) = delete;
    BeatMap(BeatMap&&)                 = default;
    BeatMap& operator=(BeatMap&&)      = default;
};
```

### Beat Time Resolution

```cpp
// Per #1533: time-source is a table-membership fact, not a row-level flag.
// Indexed entries (BeatEntry, in *_beats) resolve via beat_times[…] when
// loaded, else via the BPM grid. Timed siblings (BeatEntryTimed, in
// *_beats_timed) always carry their own time_sec.
float beat_time;
if constexpr (entry_is_timed) {                                     // *_beats_timed
    beat_time = entry.time_sec;
} else if (beat_index_in_range(map.beat_times, entry.beat_index)) { // *_beats + analysed onsets
    beat_time = map.beat_times[entry.beat_index];
} else {                                                            // *_beats + BPM grid
    beat_time = song.offset + entry.beat_index * song.beat_period;
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

- **Input System (Spec 1)** — Spec 1 owns the dispatcher events that drive the player (per-shape `ShapePress*Event`, per-direction `Go*Event`). Spec 3 does not consume those events directly; it only re-uses the `Shape` enum (label / lookup key in `app/components/player.h`) as the shared vocabulary between Spec 1's player-shape mutations and the obstacle factories' `RequiredShape*Tag` emplacement here.
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
  │  PTS_SHAPE_GATE             │ 200       │ scoring base        │
  │  PTS_SPLIT_PATH             │ 300       │ scoring base        │
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
compute_screen_transform -> input_system -> ui_update_system ->
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

`tick_playing_systems` is gated on the active `GamePhasePlayingTag`
(emplaced on `reg.ctx()` by `game_state_system`; see
`app/systems/playing_systems_runner.cpp`):

```
beat_log_system -> beat_scheduler_system -> shape_window_activation_system ->
player_movement_system -> scroll_system -> motion_system -> collision_system ->
shape_window_system -> miss_detection_system -> scoring_system
```

## Shared Enum/Type Dependencies

```
  input.h                rhythm/scoring.h        beatmap scheduling
  ──────────────         ─────────────────       ───────────────────
  InputState         ────→  player_input_system
  ShapePress*Event   ────→  player_input_system  (via entt::dispatcher sink)
  Go{Up,Down,Left,Right}Event
                     ────→  player_input_system  (via entt::dispatcher sink)
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
  │  ShapePress*Event       │ event    │ Spec 1 — Input          │
  │  Menu*Event             │ event    │ Spec 1 — Input          │
  │  Go{Up,Down,Left,Right}Event │ event│ Spec 1 — Input          │
  │  ScoreState             │ singleton│ Spec 2 — Rhythm Scoring │
  │  ScorePopup             │ per-ent  │ Spec 2 — Rhythm Scoring │
  │  TimingGrade            │ per-ent  │ Spec 2 — Rhythm Scoring │
  │  Obstacle               │ per-ent  │ Spec 3 — Scheduling     │
  │  BeatInfo               │ per-ent  │ Spec 3 — Scheduling     │
  │  RequiredShape*Tag      │ tag      │ Spec 3 — Scheduling     │
  │  int8_t (lane index)    │ per-ent  │ Spec 3 — Scheduling     │
  │  ScoredTag              │ tag      │ Spec 2/3 bridge         │
  │  MissTag                │ tag      │ Spec 2/3 bridge         │
  └─────────────────────────┴──────────┴─────────────────────────┘
```

---

*End of Feature Specifications — Mid Game Designer, SHAPESHIFTER v1.0*
