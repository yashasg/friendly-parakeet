# Tester Personas — Technical Specification
## DoD-Architect Deliverable · v1.0

> Implementation spec for the test_player system, session logging,
> and ring zone tracking. Reviewed for data-oriented design correctness.
>
> **Design Doc**: See `tester-personas.md` for persona definitions and
> expected outcomes.

> **Legacy transform naming note:** this older test-player spec still uses
> `Position`/`Velocity` in diagrams and pseudocode. Current runtime transform
> components are `WorldPosition` and `MotionVelocity`; treat the older names in
> this document as historical aliases unless the text explicitly references
> current component declarations.

```
  ┌─────────────────────────────────────────────────────────┐
  │  1. DATA INVENTORY ................................. p.1│
  │  2. SYSTEM PIPELINE ................................ p.2│
  │  3. COMPONENT DEFINITIONS .......................... p.3│
  │  4. INPUT INJECTION ................................ p.4│
  │  5. DECISION LOGIC ................................. p.5│
  │  6. SESSION LOGGING ................................ p.6│
  │  7. RING ZONE TRACKING ............................. p.7│
  │  8. DoD REVIEW FINDINGS ............................ p.8│
  │  9. FILE LAYOUT & BUILD ............................ p.9│
  └─────────────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# 1. DATA INVENTORY
# ═══════════════════════════════════════════════════

## What Data Exists (Runtime)

```
  ┌──────────────────────┬──────┬────────────┬────────────────────────┐
  │ Data                 │ Size │ Count      │ Access Pattern         │
  ├──────────────────────┼──────┼────────────┼────────────────────────┤
  │ Position             │  8B  │ ~10 max    │ HOT: every frame ×3sys │
  │ Velocity             │  8B  │ ~10 max    │ HOT: scroll_system     │
  │ Obstacle             │  4B  │ ~10 max    │ WARM: collision, decide│
  │ RequiredShape        │  1B  │ ~6 max     │ WARM: collision, decide│
  │ BlockedLanes         │  1B  │ ~3 max     │ WARM: collision, decide│
  │ RequiredLane         │  1B  │ ~2 max     │ WARM: collision, decide│
  │ BeatInfo             │ 12B  │ ~10 max    │ COLD: beat scheduling  │
  ├──────────────────────┼──────┼────────────┼────────────────────────┤
  │ TestPlayerState      │~2.6KB│ 1 singleton│ COLD: once per frame   │
  │ SessionLog           │ 16B  │ 1 singleton│ COLD: on events only   │
  │ TestPlayerConfig     │ 20B  │ 1 singleton│ COLD: read-only        │
  └──────────────────────┴──────┴────────────┴────────────────────────┘

  Max ~10 obstacles on screen at 120 BPM, 4 lead beats.
  All obstacle data fits in < 1 cache line per entity.
  TestPlayerState is singleton — no iteration, no cache concern.
```

## Data Flow (Transforms)

```
  ┌──────────────┐     ┌──────────────┐     ┌────────────────┐
  │ Position     │     │ Obstacle     │     │ BeatInfo       │
  │ (per obs)    │     │ RequiredShape│     │ arrival_time   │
  │              │     │ BlockedLanes │     │                │
  └──────┬───────┘     │ RequiredLane │     └───────┬────────┘
         │             │ Obstacle.kind│             │
         │             └──────┬───────┘             │
         ▼                    ▼                     ▼
  ┌──────────────────────────────────────────────────────────┐
  │              test_player_system                          │
  │  PERCEIVE → DECIDE → WAIT → EXECUTE                     │
  └──────────────────────┬───────────────────────────────────┘
                         │
                         ▼ writes
  ┌──────────────────────────────────────────────────────────┐
  │              InputState.key_* flags                      │
  │  (existing singleton — no new data created)              │
  └──────────────────────────────────────────────────────────┘
                         │
                         ▼ consumed by
                 semantic input event queue
                         │
                         ▼
                 player_input handlers (existing)
```


---
---
---

# ═══════════════════════════════════════════════════
# 2. SYSTEM PIPELINE
# ═══════════════════════════════════════════════════

## Insertion into Main Loop

```cpp
// ── OUTSIDE fixed timestep (once per frame) ─────────
input_system(reg, raw_dt);          // Phase 0: polls raylib
gameplay_hud_process_button_input(reg);
test_player_system(reg, raw_dt);    // Phase 0.5: ★ NEW — injects AI input

// ── INSIDE fixed timestep loop ──────────────────────
while (accumulator >= FIXED_DT) {
    game_state_system(reg, FIXED_DT);    // drains semantic input events
    song_playback_system(reg, FIXED_DT);
    tick_playing_systems(reg, FIXED_DT); // beat scheduling through scoring
    obstacle_despawn_system(reg, FIXED_DT);
    popup_feedback_system(reg, FIXED_DT);
    popup_display_system(reg, FIXED_DT);
    energy_system(reg, FIXED_DT);
    energy_bar_system(reg, FIXED_DT);
    particle_system(reg, FIXED_DT);
    accumulator -= FIXED_DT;
}
```

## System Dependency Chain

```
  input_system ──▶ gameplay_hud_process_button_input ──▶ test_player_system ──▶ game_state_system ──▶ player input handlers
       │                │                      │
    clears           writes                 reads
    key_* flags      key_* flags           key_* flags
                                            drains ShapePress*Event /
                                            Menu*Event / Go*Event

  tick_playing_systems ──▶ obstacle_despawn_system ──▶ popup/energy/particle systems
       │                              │
    beat scheduling, movement,       removes resolved/passed obstacles
    collision, miss detection,
    scoring
```


---
---
---

# ═══════════════════════════════════════════════════
# 3. COMPONENT DEFINITIONS
# ═══════════════════════════════════════════════════

## Skill Config Table (constexpr, not a component)

```cpp
// app/systems/test_player.h

// The former `enum class TestPlayerSkill { Pro, Good, Bad }` and its
// 3-row `SKILL_TABLE[]` index lookup were eradicated per issue #1202/#1204
// (Fabian existential processing). Each former enum value is now a named
// `SkillConfig` constant; the active skill is the `SkillConfig` value
// itself on `TestPlayerState::skill` (no discriminator, no index).

struct SkillConfig {
    float       vision_range;   // px from PLAYER_Y upward
    float       reaction_min;   // seconds
    float       reaction_max;   // seconds
    bool        aim_perfect;    // true = delay to hit Perfect window
    const char* name;           // CLI key + session-log key
};

inline constexpr SkillConfig SKILL_PRO  { 800.0f, 0.300f, 0.500f, true,  "pro"  };
inline constexpr SkillConfig SKILL_GOOD { 600.0f, 0.500f, 0.800f, false, "good" };
inline constexpr SkillConfig SKILL_BAD  { 400.0f, 0.800f, 1.200f, false, "bad"  };
```

**DoD note**: Table lookup, not inheritance. All personas use the same
system code with different data. No virtuals, no polymorphism.

## TestPlayerAction (value type, NOT a component)

```cpp
// Queued in TestPlayerState. Max ~8 concurrent actions.
struct TestPlayerAction {
    entt::entity obstacle    = entt::null;
    float        timer       = 0.0f;       // countdown to execution (seconds)
    float        arrival_time = 0.0f;      // song_time when obstacle reaches player

    // Required actions (determined from obstacle components)
    // The former `Shape target_shape = Shape::Hexagon` sentinel was
    // replaced by `const ShapePressSpec* shape_press` (issue #1202 PR C3):
    // nullptr means "no shape change", a non-null pointer carries both
    // the press-enqueue function and the log labels for that shape.
    const ShapePressSpec* shape_press = nullptr;
    int8_t target_lane                = -1;             // -1 = no lane change needed
    // The former `VMode target_vertical = VMode::Grounded` enum was split
    // into two independent boolean columns (issue #1202/#1204); both false
    // means "grounded — no change needed".
    bool   wants_jump                 = false;
    bool   wants_slide                = false;

    // Per-sub-action completion. Three independent boolean columns
    // (the former `ActionDoneBit` bitmask was eradicated per issue
    // #1202/#1204 — per Fabian's existential processing, each completion
    // flag is its own column).
    bool shape_done    = false;
    bool lane_done     = false;
    bool vertical_done = false;
};
```

**DoD note**: Sentinel-free choice encoding. `shape_press = nullptr` means
"no shape change needed" (the pointer's value IS the choice — identity-is-the-choice
mechanic, no enum compare). `target_lane = -1` and `!wants_jump && !wants_slide`
follow the same per-column-defaults-mean-absence pattern. Per-sub-action
completion is three independent boolean columns rather than a packed bitmask.

## TestPlayerState (context singleton)

```cpp
// app/systems/test_player.h

struct TestPlayerState {
    // ── Hot ──────────────────────────────────────────────────
    // `skill` is a `SkillConfig` row value (one of `SKILL_PRO` / `SKILL_GOOD` /
    // `SKILL_BAD`), not a discriminator — the row IS the choice.
    SkillConfig     skill   = SKILL_PRO;
    bool            active  = false;

    // Delay between consecutive lane swipes (simulates human re-swipe time)
    static constexpr float SWIPE_COOLDOWN = 0.125f;  // 125ms
    float swipe_cooldown_timer = 0.0f;

    // Action queue — fixed capacity, no heap allocation in hot path.
    static constexpr int MAX_ACTIONS = 32;
    TestPlayerAction actions[MAX_ACTIONS] = {};
    int              action_count = 0;

    // ── Warm ─────────────────────────────────────────────────
    std::mt19937     rng;
};
```

**DoD note**: `std::vector` replaced with fixed-size arrays. We know
the upper bound (~12 on-screen obstacles); `MAX_ACTIONS = 32` gives
generous headroom. Fixed arrays avoid heap allocation and give
deterministic memory layout. Planned-obstacle tracking no longer uses
an inline `planned[]` array on the singleton — perception now tags
each planned obstacle entity with the empty `TestPlayerPlannedTag`
component, letting EnTT views drive the dedup check.

## Removed ring-zone component

The older `RingZoneTracker` component was removed with the ring-zone logging
pipeline. Current test-player perception reads obstacle components directly;
runtime beat telemetry comes from `beat_log_system`.

## SessionLog (context singleton)

The runtime struct lives in `app/systems/session_logger_system.h` — see that header for
the authoritative definition. Beyond `FILE* file` and `uint32_t frame`, it
also owns a per-frame `std::string buffer` (pre-reserved at
`kMaxLogBufferBytes = 4096`, so the hot write path never reallocates) and a
`last_logged_beat` cursor used by `beat_log_system`. The struct is move-only
with an explicit destructor / `release()` that closes the file handle.

Helper functions are free functions, not methods. The write entry point is
`session_log_write`, paired with `session_log_open` / `session_log_close` /
`session_log_flush` / `session_log_begin_frame`:

`void session_log_write(SessionLog&, float song_time, const char* tag, const char* fmt, ...);`


---
---
---

# ═══════════════════════════════════════════════════
# 4. INPUT INJECTION
# ═══════════════════════════════════════════════════

## Injection Target: EnTT Dispatcher

`test_player_system` does not poke hardware-facing input state. It enqueues
the same semantic events (`ShapePress*Event`, `Menu*Event`, `Go*Event`) that
the real `input_system` produces from keyboard / touch / mouse, then lets the
rest of the pipeline run unchanged:

```
  test_player enqueues:                 consumed by:
  ─────────────────────────             ───────────────────────────────
  ShapePressCircleEvent                 player_input_system → ShapeWindow
  ShapePressTriangleEvent               player_input_system → ShapeWindow
  ShapePressSquareEvent                 player_input_system → ShapeWindow
  MenuConfirmEvent                      game_state_system  → phase transitions
  MenuRestartEvent                      game_state_system  → phase transitions
  GoLeftEvent / GoRightEvent            player_movement_system → Lane change
  GoUpEvent / GoDownEvent               player_movement_system → Jump / Slide
```

Because the injection target is the dispatcher (not a platform-guarded
`InputState` field), `test_player_system` builds and runs on every platform —
there is no `#ifdef PLATFORM_DESKTOP` stub required.

## One Action Per Frame (test_player choice)

The execute loop in `test_player_system.cpp` injects at most one action per
frame:

```cpp
  bool key_injected = false;
  for (int ei = 0; ei < state->action_count && !key_injected; ++ei) {
      // Priority 1: Shape press → enqueue ShapePress*Event,    set key_injected
      // Priority 2: Lane change → enqueue Go{Left,Right}Event, set key_injected
      // Priority 3: Vertical    → enqueue Go{Up,Down}Event,    set key_injected
  }
```

Actions are processed in arrival-time order so the closest obstacle wins.
The shape-first / lane-second / vertical-third priority and the one-action-
per-frame cap are deliberate human-plausibility choices (one fingertip per
frame, shape press first because morph is the slower verb), not a workaround
for any downstream early-return. The dispatcher itself is a queue and accepts
multiple events per frame; the constraint lives entirely inside test_player.

## Fixed Timestep Idempotency

test_player runs OUTSIDE the fixed timestep, so a single enqueue may flow
through two fixed-step iterations within one render frame. Verified safe:
each `ButtonPressEvent` fires once (the dispatcher drains its queue per
pump), and `player_input_system` checks the current `ShapeWindow.phase`
before mutating — re-entry on a window that is already MorphIn / Active is
a no-op.

## Auto-Start (Title / GameOver / SongComplete screens)

```cpp
  if (reg.ctx().contains<GamePhaseTitleTag>() ||
      reg.ctx().contains<GamePhaseGameOverTag>() ||
      reg.ctx().contains<GamePhaseSongCompleteTag>()) {
      if (gs.phase_timer > constants::TEST_PLAYER_AUTO_START_DELAY) {
          // Restart on end screens, Confirm on Title — same dispatcher path
          // a real menu button press would take.
          disp.enqueue<ButtonPressEvent>({ButtonPressKind::Menu, Shape::Circle,
                                          target_action, 0});
      }
  }
```


---
---
---

# ═══════════════════════════════════════════════════
# 5. DECISION LOGIC
# ═══════════════════════════════════════════════════

## Action Determination (per obstacle)

```
  determine_action(registry, obstacle_entity, player_lane):

  ┌─────────────────────────────────────────────────────────────────┐
  │                                                                 │
  │  Has RequiredShape?                                             │
  │    YES → target_shape = required.shape                          │
  │                                                                 │
  │  Has BlockedLanes?                                              │
  │    YES → is current lane blocked?                               │
  │      YES → target_lane = nearest unblocked lane                 │
  │      NO  → target_lane stays -1 (no change)                     │
  │                                                                 │
  │  Has RequiredLane?                                               │
  │    YES → target_lane = required.lane                             │
  │                                                                 │
  └─────────────────────────────────────────────────────────────────┘
```

## Nearest Unblocked Lane Selection

```
  blocked_mask = 0b101  (lanes 0 and 2 blocked)
  player at lane 1

  Lane 0: (0b101 >> 0) & 1 = 1  BLOCKED  ✗
  Lane 1: (0b101 >> 1) & 1 = 0  OPEN     ✓ ← nearest to player
  Lane 2: (0b101 >> 2) & 1 = 1  BLOCKED  ✗

  Algorithm: iterate lanes by distance from current, pick first unblocked.
```

## Reaction Timer (Score Optimization)

```
  Pro player (aim_perfect = true):
  ──────────────────────────────────
  ideal_press = arrival_time − morph_duration − half_window
  time_until_ideal = ideal_press − song_time
  if time_until_ideal > reaction_min:
      timer = time_until_ideal          ← deliberate delay for Perfect
  else:
      timer = uniform(reaction_min, reaction_max)  ← too late, react ASAP

  Good / Bad player (aim_perfect = false):
  ──────────────────────────────────
  timer = uniform(reaction_min, reaction_max)   ← always react ASAP
```

## Multi-Frame Execution (Lane Changes)

```
  Player at lane 0, needs lane 2:

  Frame N:    key_d → lane.target = 1, lerp_t = 0
  Frame N+5:  transition complete, lane.current = 1, lane.target = -1
  Frame N+6:  key_d → lane.target = 2, lerp_t = 0
  Frame N+11: transition complete, lane.current = 2

  Guard: only inject key_a/d when lane.target == -1
  (no transition in progress). Prevents restarting a mid-transition.

  Total: ~0.17s (11 frames at 60fps)
```

## Action Priority (when timer fires)

```
  1. Shape change   (highest — needs morph_duration to activate)
  2. Lane change    (fast — ~0.083s per lane)
  3. Vertical action (jump/slide — instant trigger)

  Only ONE key injected per frame. Next action waits for next frame.
```


---
---
---

# ═══════════════════════════════════════════════════
# 6. SESSION LOGGING
# ═══════════════════════════════════════════════════

## Log Format

```
  [F:NNNN T:SS.SSS] [TAG   ] EVENT details...

  F = frame number (uint32)
  T = song_time (seconds, from SongState)
  TAG = PLAYER or GAME (6-char padded)
```

## [PLAYER] Events (written by test_player_system)

```
  PERCEIVE    obstacle enters vision range
  PLAN        action determined + reaction timer set
  WAIT        pro player deliberately delays (logged once)
  EXECUTE     input key injected
  AUTO_START  tap to start from Title/GameOver
```

## [GAME] Events (written by EnTT signals and runtime systems)

```
  SONG_START       playing phase entered
  OBSTACLE_SPAWN   ObstacleTag emplaced (via on_construct signal)
  COLLISION        ScoredTag emplaced (via on_construct signal)
  SCORE            points awarded
  SONG_END         song finishes or player dies
```

## EnTT Signal Hooks (non-invasive)

```cpp
  // Registered during test_player setup ONLY:
  reg.on_construct<ObstacleTag>().connect<&on_obstacle_spawn>();
  reg.on_construct<ScoredTag>().connect<&on_scored>();

  // Signal handler signature:
  void on_obstacle_spawn(entt::registry& reg, entt::entity entity) {
      auto* log = reg.ctx().find<SessionLog>();
      if (!log || !log->file) return;
      // read Obstacle, BeatInfo, RequiredShape from entity
      // write [GAME] OBSTACLE_SPAWN line
  }
```

**DoD note**: `on_construct<ObstacleTag>` observes spawn metadata only. It does
not add ring-zone tracking components in the current runtime.

## Log File Naming

```
  session_{skill}_{YYYYMMDD_HHMMSS}.log
  Example: session_pro_20260411_050600.log
```

## Example Session

```
══════ Test Session: PRO | beatmap: 2_drama | 2026-04-11 05:06 ══════

[F:0001 T:0.000] [GAME  ] SONG_START bpm=120 beats=48 difficulty=medium
[F:0060 T:1.000] [GAME  ] OBSTACLE_SPAWN beat=4 kind=ShapeGate shape=Circle lane=1
[F:0090 T:1.500] [PLAYER] PERCEIVE obstacle=5 kind=ShapeGate shape=Circle lane=1 dist=520px
[F:0090 T:1.500] [PLAYER] PLAN action=shape_circle react=0.387s ideal_press=2.600s
[F:0090 T:1.500] [PLAYER] WAIT delaying 1.100s (aiming for Perfect)
[F:0156 T:2.600] [PLAYER] EXECUTE key_1(Circle) for obstacle=5
[F:0180 T:3.000] [GAME  ] COLLISION obstacle=5 result=CLEAR timing=Perfect(0.92)
[F:0180 T:3.000] [GAME  ] SCORE +300 (200 × 1.50timing) chain=1 total=300
```


---
---
---

# ═══════════════════════════════════════════════════
# 7. RING ZONE TRACKING
# ═══════════════════════════════════════════════════

## Zone Boundary Distances

```
  When player presses at distance D from obstacle:
    pct_from_peak = |D/scroll_speed − morph_duration − half_window| / half_window

  Zone boundaries:
    D = v × (M + factor × H)

  v = scroll_speed,  M = morph_duration,  H = half_window
```

```
  ┌────────────────┬────────┬─────────────┬───────────────────┐
  │  Zone          │ Factor │ 120BPM Dist │ pct_from_peak     │
  ├────────────────┼────────┼─────────────┼───────────────────┤
  │  Window edge   │ 2.00   │  468 px     │ 1.00              │
  │  Bad  (early)  │ 1.75   │  416 px     │ 0.75              │
  │  Ok   (early)  │ 1.50   │  364 px     │ 0.50              │
  │  Good (early)  │ 1.25   │  312 px     │ 0.25              │
  │  Perfect ★     │ 1.00   │  260 px     │ 0.00  (center)    │
  │  Good (late)   │ 0.75   │  208 px     │ 0.25              │
  │  Ok   (late)   │ 0.50   │  156 px     │ 0.50              │
  │  Bad  (late)   │ 0.25   │  104 px     │ 0.75              │
  │  Window edge   │ 0.00   │   52 px     │ 1.00              │
  └────────────────┴────────┴─────────────┴───────────────────┘
```

## Removed ring-zone pipeline

The older `ring_zone_log_system`/`RingZoneTracker` design is no longer part of
the runtime pipeline. Beat telemetry is recorded by `beat_log_system`, authored
obstacles are spawned by `beat_scheduler_system`, and collision/scoring run
inside `tick_playing_systems`.


---
---
---

# ═══════════════════════════════════════════════════
# 8. DoD REVIEW FINDINGS
# ═══════════════════════════════════════════════════

```
  ┌─────┬────────────────────────────────────────────────────────┐
  │  #  │  Finding                                               │
  ├─────┼────────────────────────────────────────────────────────┤
  │ 🟢1 │  test_player injects at most one action per frame in   │
  │     │  Shape → Lane → Vertical priority order, enforced by   │
  │     │  the `key_injected` guard in the execute loop. This is │
  │     │  a deliberate human-plausibility choice, not a         │
  │     │  workaround for any downstream early-return — the      │
  │     │  EnTT dispatcher is a multi-event queue.               │
  ├─────┼────────────────────────────────────────────────────────┤
  │ 🟢2 │  test_player_system enqueues semantic events directly  │
  │     │  on the dispatcher (ShapePress*Event / Menu*Event /    │
  │     │  Go*Event), so it is platform-portable. No             │
  │     │  InputState.key_* fields, no PLATFORM_DESKTOP compile  │
  │     │  guard required.                                       │
  ├─────┼────────────────────────────────────────────────────────┤
  │ 🟡3 │  Original design used std::vector for action queue     │
  │     │  and planned list. Heap allocation in a per-frame      │
  │     │  system, even if small.                                │
  │     │  FIX: Fixed-size action queue (MAX_ACTIONS=32) and a   │
  │     │  per-entity TestPlayerPlannedTag for planning dedup.   │
  ├─────┼────────────────────────────────────────────────────────┤
  │ 🟡4 │  Original design used 7 separate bool fields for      │
  │     │  need_shape, need_lane, need_jump, need_slide,         │
  │     │  shape_done, lane_done, vertical_done.                 │
  │     │  FIX: Sentinel values (Hexagon = no shape change,      │
  │     │  -1 = no lane change, Grounded = no vertical). Done    │
  │     │  flags packed into uint8_t bitmask.                    │
  ├─────┼────────────────────────────────────────────────────────┤
  │ 🟢5 │  RingZoneTracker/ring_zone_log_system were removed.    │
  │     │  Current beat telemetry uses beat_log_system and the    │
  │     │  collision/scoring path in tick_playing_systems.        │
  ├─────┼────────────────────────────────────────────────────────┤
  │ 🟢6 │  Key flags persist across multiple fixed-step          │
  │     │  iterations within one frame. Verified idempotent:     │
  │     │  input event dispatch re-fires but player input        │
  │     │  phase check prevents double-processing.               │
  ├─────┼────────────────────────────────────────────────────────┤
  │ 🟢7 │  std::mt19937 is 2.5KB — large but acceptable for a   │
  │     │  singleton context component. Not iterated.            │
  ├─────┼────────────────────────────────────────────────────────┤
  │ 🟢8 │  No new hot-path components. test_player reads         │
  │     │  existing Position (8B) and Obstacle (4B).             │
  │     │  Removed ring-zone tracking keeps cache impact low.    │
  └─────┴────────────────────────────────────────────────────────┘
```


---
---
---

# ═══════════════════════════════════════════════════
# 9. FILE LAYOUT & BUILD
# ═══════════════════════════════════════════════════

## File Layout

```
  app/
    systems/
      test_player.h            ← SkillConfig + named SKILL_PRO/GOOD/BAD constants,
      │                           TestPlayerAction (sentinel-free choice encoding),
      │                           TestPlayerState (context singleton). No
      │                           `TestPlayerSkill` enum exists in app/ today.
      test_player_system.cpp   ← AI: perceive, decide, wait, execute
      │                           Platform-portable: enqueues events
      │                           directly on the EnTT dispatcher
      all_systems.h            ← System declarations
    session_logger.h           ← SessionLog struct, session_log_write() and friends
    session_logger.cpp         ← File I/O, EnTT signal handlers, formatting
    main.cpp                   ← CLI arg parsing, setup, system insertion
```

## CMakeLists.txt Changes

```cmake
  # Add to shapeshifter_lib sources:
  app/systems/test_player_system.cpp
  app/systems/session_logger_system.cpp
```

## CLI Activation

```bash
  ./build/shapeshifter --test-player pro
  ./build/shapeshifter --test-player good
  ./build/shapeshifter --test-player bad
```

Requires uncommenting `argc/argv` in `main()` signature and adding
a simple string comparison parser.
