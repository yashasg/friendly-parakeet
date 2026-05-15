# Tester Personas — Technical Specification
## DoD-Architect Deliverable · v1.0

> Implementation spec for the test_player system, session logging,
> and ring zone tracking. Reviewed for data-oriented design correctness.
>
> **Design Doc**: See `tester-personas.md` for persona definitions and
> expected outcomes.

> **Legacy transform naming note:** this older test-player spec still uses
> `Position`/`Velocity` in diagrams and pseudocode. Current runtime transform
> components are `WorldTransform` and `MotionVelocity`; treat the older names in
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
                                            drains ButtonPressEvent /
                                            GoEvent

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
// app/components/test_player.h

enum class TestPlayerSkill : uint8_t { Pro = 0, Good = 1, Bad = 2 };

struct SkillConfig {
    float vision_range;     // px from PLAYER_Y upward
    float reaction_min;     // seconds
    float reaction_max;     // seconds
    bool  aim_perfect;      // true = delay to hit Perfect window
};

static constexpr SkillConfig SKILL_TABLE[] = {
    // Pro:   fast reflexes, wide vision, aims for Perfect
    { 800.0f, 0.300f, 0.500f, true  },
    // Good:  moderate reflexes, medium vision, reacts ASAP
    { 600.0f, 0.500f, 0.800f, false },
    // Bad:   slow reflexes, narrow vision, reacts ASAP
    { 400.0f, 0.800f, 1.200f, false },
};
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
    Shape  target_shape      = Shape::Hexagon; // Hexagon = no shape change needed
    int8_t target_lane       = -1;             // -1 = no lane change needed
    VMode  target_vertical   = VMode::Grounded;// Grounded = no jump/slide needed

    // Execution state (packed into uint8_t bitmask)
    //   bit 0: shape_done
    //   bit 1: lane_done
    //   bit 2: vertical_done
    uint8_t done_flags       = 0;
};
```

**DoD note**: Bools replaced with bitmask. `target_shape = Hexagon` means
"no change needed" — existential encoding via sentinel value instead of a
separate `need_shape` boolean. Same pattern for `target_lane = -1` and
`target_vertical = Grounded`.

## TestPlayerState (context singleton)

```cpp
// app/components/test_player.h

struct TestPlayerState {
    // ── Hot ──────────────────────────────────────────────────
    TestPlayerSkill skill   = TestPlayerSkill::Pro;
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

The runtime struct lives in `app/util/session_logger.h` — see that header for
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

## Injection Target: InputState Keyboard Flags

```
  test_player writes:         gesture_system reads:
  ─────────────────          ──────────────────────
  InputState.key_1  ───────▶  ShapeButtonEvent(Circle)
  InputState.key_2  ───────▶  ShapeButtonEvent(Triangle)
  InputState.key_3  ───────▶  ShapeButtonEvent(Square)
  InputState.key_a  ───────▶  GestureResult(SwipeLeft)
  InputState.key_d  ───────▶  GestureResult(SwipeRight)
  InputState.key_w  ───────▶  GestureResult(SwipeUp / Jump)
  InputState.key_s  ───────▶  GestureResult(SwipeDown / Slide)
```

## ⚠ CRITICAL: One Key Per Frame Constraint

`gesture_system` uses **early-return** on the first matching key:

```cpp
  // gesture_system.cpp lines 20-26 — ACTUAL CODE:
  if (input.key_w) { gesture.gesture = SwipeGesture::SwipeUp;    return; }
  if (input.key_s) { gesture.gesture = SwipeGesture::SwipeDown;  return; }
  if (input.key_a) { gesture.gesture = SwipeGesture::SwipeLeft;  return; }
  if (input.key_d) { gesture.gesture = SwipeGesture::SwipeRight; return; }
  if (input.key_1) { btn_evt.pressed = true; ... return; }
  if (input.key_2) { btn_evt.pressed = true; ... return; }
  if (input.key_3) { btn_evt.pressed = true; ... return; }
```

**Consequence**: Setting both `key_a` AND `key_1` in the same frame
results in ONLY `SwipeLeft` being processed. The shape change is LOST.

**Rule**: Test player must set **exactly ONE key flag per frame**.

```
  ComboGate (shape=Circle, lane=0):
  ┌────────────────────────────────────────────────┐
  │ Frame N:   key_1 = true   → ShapeButtonEvent  │  shape first (slower)
  │ Frame N+1: key_a = true   → GestureResult     │  lane second (fast)
  │                                                │
  │ Shape goes first because morph_duration is     │
  │ the bottleneck (~0.1s to activate).            │
  │ Lane switch completes in ~0.083s.              │
  └────────────────────────────────────────────────┘
```

## ⚠ CRITICAL: PLATFORM_DESKTOP Compile Guard

`InputState.key_*` fields are behind `#ifdef PLATFORM_DESKTOP`:

```cpp
  // input.h line 25-36
  #ifdef PLATFORM_DESKTOP
      bool key_w = false;
      // ...
  #endif
```

**Requirement**: Test player feature requires `PLATFORM_DESKTOP` to be
defined at compile time. The test player system file must be wrapped:

```cpp
  #ifdef PLATFORM_DESKTOP
  void test_player_system(entt::registry& reg, float dt) { ... }
  #else
  void test_player_system(entt::registry&, float) {}  // no-op stub
  #endif
```

## Fixed Timestep Idempotency

test_player runs OUTSIDE the fixed timestep. Key flags persist across
multiple iterations of gesture_system within one frame. Verified safe:

```
  Iteration 1: gesture_system reads key_1 → sets ShapeButtonEvent
               player_input_handle_press reads ButtonPressEvent → MorphIn
  Iteration 2: gesture_system reads key_1 again → sets ShapeButtonEvent
               player_input_handle_press checks phase → now MorphIn, not Idle
               → Idle branch fails → no-op ✓ (idempotent)
```

## Auto-Start (Title / GameOver screens)

```cpp
  if (gs.phase == GamePhase::Title || gs.phase == GamePhase::GameOver) {
      if (gs.phase_timer > 0.5f) {
          input.touch_up = true;   // triggers phase transition
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
  │ 🔴1 │  gesture_system early-returns on first key match.      │
  │     │  Shape+movement in same frame is IMPOSSIBLE through    │
  │     │  InputState keys. Test player must inject exactly      │
  │     │  ONE key per frame.                                    │
  │     │  FIX: Documented in §4. Priority: shape first.         │
  ├─────┼────────────────────────────────────────────────────────┤
  │ 🔴2 │  InputState.key_* fields are #ifdef PLATFORM_DESKTOP.  │
  │     │  Test player won't compile on web/mobile.              │
  │     │  FIX: Wrap test_player_system in #ifdef PLATFORM_      │
  │     │  DESKTOP with a no-op stub for other platforms.        │
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
    components/
      test_player.h            ← TestPlayerSkill, SkillConfig, TestPlayerAction,
      │                           TestPlayerState (context singleton)
    systems/
      test_player_system.cpp   ← AI: perceive, decide, wait, execute
      │                           Guarded: #ifdef PLATFORM_DESKTOP
      all_systems.h            ← System declarations
    session_logger.h           ← SessionLog struct, session_log_write() and friends
    session_logger.cpp         ← File I/O, EnTT signal handlers, formatting
    main.cpp                   ← CLI arg parsing, setup, system insertion
```

## CMakeLists.txt Changes

```cmake
  # Add to shapeshifter_lib sources:
  app/systems/test_player_system.cpp
  app/util/session_logger.cpp
```

## CLI Activation

```bash
  ./build/shapeshifter --test-player pro
  ./build/shapeshifter --test-player good
  ./build/shapeshifter --test-player bad
```

Requires uncommenting `argc/argv` in `main()` signature and adding
a simple string comparison parser.
