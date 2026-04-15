# Tester Personas — Technical Specification
## DoD-Architect Deliverable · v1.0

> Implementation spec for the test_player system, session logging,
> and ring zone tracking. Reviewed for data-oriented design correctness.
>
> **Design Doc**: See `tester-personas.md` for persona definitions and
> expected outcomes.

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
  │ RequiredVAction      │  1B  │ ~2 max     │ WARM: collision, decide│
  │ BeatInfo             │ 12B  │ ~10 max    │ COLD: ring_zone only   │
  │ RingZoneTracker      │  2B  │ ~6 max     │ WARM: ring_zone_log    │
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
         │             │ RequiredVAct │             │
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
                 gesture_system (existing)
                         │
                         ▼
                 player_action_system (existing)
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
test_player_system(reg, raw_dt);    // Phase 0.5: ★ NEW — injects AI input

// ── INSIDE fixed timestep loop ──────────────────────
while (accumulator >= FIXED_DT) {
    gesture_system(reg, FIXED_DT);       // reads InputState → GestureResult
    game_state_system(reg, FIXED_DT);
    song_playback_system(reg, FIXED_DT);
    beat_scheduler_system(reg, FIXED_DT);
    player_action_system(reg, FIXED_DT);
    shape_window_system(reg, FIXED_DT);
    player_movement_system(reg, FIXED_DT);
    difficulty_system(reg, FIXED_DT);
    obstacle_spawn_system(reg, FIXED_DT);
    scroll_system(reg, FIXED_DT);
    ring_zone_log_system(reg, FIXED_DT); // Phase 5.1: ★ NEW — after scroll
    burnout_system(reg, FIXED_DT);
    collision_system(reg, FIXED_DT);
    scoring_system(reg, FIXED_DT);
    hp_system(reg, FIXED_DT);
    lifetime_system(reg, FIXED_DT);
    particle_system(reg, FIXED_DT);
    cleanup_system(reg, FIXED_DT);
    accumulator -= FIXED_DT;
}
```

## System Dependency Chain

```
  input_system ──▶ test_player_system ──▶ gesture_system ──▶ player_action_system
       │                │                      │
    clears           writes                 reads
    key_* flags      key_* flags           key_* flags
                                            writes GestureResult /
                                            ShapeButtonEvent

  scroll_system ──▶ ring_zone_log_system ──▶ burnout_system ──▶ collision_system
       │                │                         │                  │
    updates          reads Position             reads Position     resolves
    Position         logs [GAME] RING_ZONE      updates BurnoutState  pass/fail
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
    TestPlayerSkill skill    = TestPlayerSkill::Pro;
    bool            active   = false;
    uint32_t        frame_count = 0;

    // Action queue — fixed capacity, no heap allocation in hot path.
    // Max obstacles on screen at 120 BPM / 4 lead beats ≈ 8-12.
    static constexpr int MAX_ACTIONS = 16;
    TestPlayerAction actions[MAX_ACTIONS];
    int              action_count = 0;

    // Planned obstacle tracking — linear scan is faster than hash
    // for N < 50 due to cache locality.
    static constexpr int MAX_PLANNED = 64;
    entt::entity     planned[MAX_PLANNED];
    int              planned_count = 0;

    // RNG (2.5KB — acceptable for singleton)
    std::mt19937     rng;
};
```

**DoD note**: `std::vector` replaced with fixed-size arrays. We know
the upper bound (~12 on-screen obstacles). Fixed arrays avoid heap
allocation and give deterministic memory layout. Linear scan over 16
entries fits in a single cache line of indices.

## RingZoneTracker (per-obstacle component)

```cpp
// app/components/ring_zone.h

struct RingZoneTracker {
    int8_t last_zone   = -1;     // -1=none, 1=bad, 2=ok, 3=good, 4=perfect
    bool   past_center = false;  // true once obstacle passes Perfect center
};
// Size: 2 bytes. Emplaced ONLY on obstacles with RequiredShape.
```

**DoD note**: Only emplaced on shape-gated obstacles (ShapeGate, ComboGate,
SplitPath) — not on LaneBlock/LowBar/HighBar/LanePushLeft/LanePushRight which have no ring visual.
LanePush obstacles are passive (auto-fired), so the test player does not need to react to them.
Existential processing: if the component exists, the system processes it.

## SessionLog (context singleton)

```cpp
// app/session_logger.h

struct SessionLog {
    FILE*    file  = nullptr;
    uint32_t frame = 0;
};
```

Kept minimal — helper functions are free functions, not methods.
Write function: `void session_log(SessionLog&, float song_time, const char* tag, const char* fmt, ...)`.


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
               player_action_system reads ShapeButtonEvent → MorphIn
  Iteration 2: gesture_system reads key_1 again → sets ShapeButtonEvent
               player_action_system checks phase → now MorphIn, not Idle
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
  │  Has RequiredVAction?                                           │
  │    Jumping → target_vertical = VMode::Jumping                   │
  │    Sliding → target_vertical = VMode::Sliding                   │
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

## [GAME] Events (written by EnTT signals + ring_zone_log_system)

```
  SONG_START       playing phase entered
  OBSTACLE_SPAWN   ObstacleTag emplaced (via on_construct signal)
  RING_APPEAR      obstacle enters approach distance (has RequiredShape)
  RING_ZONE        ring crosses timing zone boundary
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
      // if has RequiredShape: emplace RingZoneTracker
  }
```

**DoD note**: on_construct for ObstacleTag is also where we emplace
RingZoneTracker — but ONLY when RequiredShape is present. This avoids
a separate system to do the emplacement.

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
[F:0060 T:1.000] [GAME  ] RING_APPEAR shape=Circle obstacle=5 dist=1040px
[F:0090 T:1.500] [PLAYER] PERCEIVE obstacle=5 kind=ShapeGate shape=Circle lane=1 dist=520px
[F:0090 T:1.500] [PLAYER] PLAN action=shape_circle react=0.387s ideal_press=2.600s
[F:0090 T:1.500] [PLAYER] WAIT delaying 1.100s (aiming for Perfect)
[F:0120 T:2.000] [GAME  ] RING_ZONE obstacle=5 zone=Bad(early) shape=Circle dist=468px
[F:0132 T:2.200] [GAME  ] RING_ZONE obstacle=5 zone=Ok(early) shape=Circle dist=416px
[F:0144 T:2.400] [GAME  ] RING_ZONE obstacle=5 zone=Good(early) shape=Circle dist=364px
[F:0150 T:2.500] [GAME  ] RING_ZONE obstacle=5 zone=Perfect shape=Circle dist=312px
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

## ring_zone_log_system

Runs after scroll_system, before burnout_system. Iterates ONLY
obstacles that have both Position and RingZoneTracker:

```cpp
  void ring_zone_log_system(entt::registry& reg, float /*dt*/) {
      auto* log = reg.ctx().find<SessionLog>();
      if (!log || !log->file) return;

      auto* song = reg.ctx().find<SongState>();
      if (!song) return;

      float v = song->scroll_speed;
      float M = song->morph_duration;
      float H = song->half_window;

      // Precompute zone boundary distances
      float dist_bad_far  = v * (M + 1.75f * H);
      float dist_ok_far   = v * (M + 1.50f * H);
      float dist_good_far = v * (M + 1.25f * H);
      float dist_perf     = v * (M + H);           // center
      float dist_good_near= v * (M + 0.50f * H);
      float dist_ok_near  = v * (M + 0.25f * H);
      float dist_bad_near = v * M;

      auto player_y = constants::PLAYER_Y;

      auto view = reg.view<Position, RingZoneTracker, RequiredShape>(
          entt::exclude<ScoredTag>);

      for (auto [entity, pos, tracker, req] : view.each()) {
          float dist = player_y - pos.y;
          if (dist <= 0.0f) continue;

          // Determine current zone
          int8_t zone = compute_zone(dist, tracker.past_center,
                                     dist_bad_far, dist_ok_far,
                                     dist_good_far, dist_perf,
                                     dist_good_near, dist_ok_near,
                                     dist_bad_near);

          // Log on zone transition
          if (zone != tracker.last_zone) {
              // ... write [GAME] RING_ZONE
              tracker.last_zone = zone;
          }

          if (dist <= dist_perf && !tracker.past_center) {
              tracker.past_center = true;
          }
      }
  }
```

**DoD note**: View queries exactly {Position, RingZoneTracker, RequiredShape}.
No extra components loaded. Excludes ScoredTag (already resolved obstacles).
Linear iteration over contiguous storage — hardware prefetcher friendly.


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
  │     │  FIX: Fixed-size arrays (MAX_ACTIONS=16, MAX_PLANNED   │
  │     │  =64). Upper bounds known from BPM analysis.           │
  ├─────┼────────────────────────────────────────────────────────┤
  │ 🟡4 │  Original design used 7 separate bool fields for      │
  │     │  need_shape, need_lane, need_jump, need_slide,         │
  │     │  shape_done, lane_done, vertical_done.                 │
  │     │  FIX: Sentinel values (Hexagon = no shape change,      │
  │     │  -1 = no lane change, Grounded = no vertical). Done    │
  │     │  flags packed into uint8_t bitmask.                    │
  ├─────┼────────────────────────────────────────────────────────┤
  │ 🟡5 │  Original design emplaced RingZoneTracker on ALL       │
  │     │  obstacles. Only RequiredShape obstacles have a ring    │
  │     │  visual. LaneBlock/LowBar/HighBar/LanePush would be     │
  │     │  for nothing.                                          │
  │     │  FIX: Emplace only on obstacles with RequiredShape.    │
  │     │  ring_zone_log_system queries {Position,               │
  │     │  RingZoneTracker, RequiredShape}.                      │
  ├─────┼────────────────────────────────────────────────────────┤
  │ 🟢6 │  Key flags persist across multiple fixed-step          │
  │     │  iterations within one frame. Verified idempotent:     │
  │     │  gesture_system re-fires but player_action_system      │
  │     │  phase check prevents double-processing.               │
  ├─────┼────────────────────────────────────────────────────────┤
  │ 🟢7 │  std::mt19937 is 2.5KB — large but acceptable for a   │
  │     │  singleton context component. Not iterated.            │
  ├─────┼────────────────────────────────────────────────────────┤
  │ 🟢8 │  No new hot-path components. test_player reads         │
  │     │  existing Position (8B) and Obstacle (4B).             │
  │     │  RingZoneTracker (2B) on ~6 entities = 12B total.      │
  │     │  Negligible cache impact.                              │
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
      ring_zone.h              ← RingZoneTracker (2B per-obstacle component)
    systems/
      test_player_system.cpp   ← AI: perceive, decide, wait, execute
      │                           Guarded: #ifdef PLATFORM_DESKTOP
      ring_zone_log_system.cpp ← Zone tracking + [GAME] RING_ZONE logging
      all_systems.h            ← Add declarations for both new systems
    session_logger.h           ← SessionLog struct, session_log() free function
    session_logger.cpp         ← File I/O, EnTT signal handlers, formatting
    main.cpp                   ← CLI arg parsing, setup, system insertion
```

## CMakeLists.txt Changes

```cmake
  # Add to shapeshifter_lib sources:
  app/systems/test_player_system.cpp
  app/systems/ring_zone_log_system.cpp
  app/session_logger.cpp
```

## CLI Activation

```bash
  ./build/shapeshifter --test-player pro
  ./build/shapeshifter --test-player good
  ./build/shapeshifter --test-player bad
```

Requires uncommenting `argc/argv` in `main()` signature and adding
a simple string comparison parser.
