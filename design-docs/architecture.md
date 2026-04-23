# SHAPESHIFTER — Technical Architecture Document
## Data-Oriented Design · C++20 · raylib · EnTT v3.16.0

> **Guiding principle**: Data lives in flat structs. Logic lives in free functions.
> The `entt::registry` is the single source of truth. No globals. No virtuals.

---

## Table of Contents

1. [Coordinate System & Constants](#1-coordinate-system--constants)
2. [Complete Component Registry](#2-complete-component-registry)
3. [System Execution Order](#3-system-execution-order)
4. [Game State Machine](#4-game-state-machine)
5. [Entity Archetypes](#5-entity-archetypes)
6. [Main Loop Structure](#6-main-loop-structure)
7. [Data Flow Diagrams](#7-data-flow-diagrams)
8. [Memory & Performance Considerations](#8-memory--performance-considerations)

---

## 1. Coordinate System & Constants

```
Portrait mode. Logical resolution scales to device via raylib virtual resolution.

    ┌─────────────────────────────┐
    │ (0,0)              (720,0)  │
    │                             │
    │   Lane 0   Lane 1   Lane 2  │
    │   x=180    x=360    x=540   │
    │                             │
    │          obstacles          │
    │          scroll ↓           │
    │                             │
    │          ● player           │
    │          y=920              │
    │─────────────────────────────│
    │   burnout meter  y=1020     │
    │─────────────────────────────│
    │  [ ● ]   [ ■ ]   [ ▲ ]     │
    │          y=1180              │
    │ (0,1280)          (720,1280)│
    └─────────────────────────────┘
```

```cpp
// constants.h — all top-level tuning knobs in one place

namespace constants {

    // ── Logical Resolution ────────────────────────────
    constexpr int   SCREEN_W          = 720;
    constexpr int   SCREEN_H          = 1280;

    // ── Lanes ─────────────────────────────────────────
    constexpr int   LANE_COUNT        = 3;
    constexpr float LANE_X[3]         = { 180.0f, 360.0f, 540.0f };
    constexpr float LANE_SWITCH_SPEED = 12.0f;   // lerp factor per second

    // ── Player ────────────────────────────────────────
    constexpr float PLAYER_Y          = 920.0f;
    constexpr float PLAYER_SIZE       = 64.0f;
    constexpr float JUMP_HEIGHT       = 160.0f;
    constexpr float JUMP_DURATION     = 0.45f;   // seconds
    constexpr float SLIDE_DURATION    = 0.50f;
    constexpr float MORPH_DURATION    = 0.12f;   // shape transition anim

    // ── Obstacles ─────────────────────────────────────
    constexpr float SPAWN_Y           = -120.0f;  // off-screen top
    constexpr float DESTROY_Y         = 1400.0f;  // off-screen bottom
    constexpr float BASE_SCROLL_SPEED = 400.0f;   // pixels/sec at ×1.0

    // ── Burnout Zones (distance from player, in px) ──
    constexpr float ZONE_SAFE_MAX     = 700.0f;   // > this = no threat
    constexpr float ZONE_SAFE_MIN     = 500.0f;
    constexpr float ZONE_RISKY_MIN    = 300.0f;
    constexpr float ZONE_DANGER_MIN   = 140.0f;
    constexpr float ZONE_DEAD_MIN     = 0.0f;     // collision line

    // ── Burnout Multipliers ───────────────────────────
    constexpr float MULT_NONE         = 1.0f;
    constexpr float MULT_SAFE         = 1.0f;
    constexpr float MULT_RISKY        = 1.5f;
    constexpr float MULT_DANGER       = 3.0f;
    constexpr float MULT_CLUTCH       = 5.0f;     // last possible frames

    // ── Scoring ───────────────────────────────────────
    constexpr int   PTS_SHAPE_GATE    = 200;
    constexpr int   PTS_LANE_BLOCK    = 100;
    constexpr int   PTS_LOW_BAR       = 100;
    constexpr int   PTS_HIGH_BAR      = 100;
    constexpr int   PTS_COMBO_GATE    = 200;
    constexpr int   PTS_SPLIT_PATH    = 300;
    constexpr int   PTS_LANE_PUSH     = 0;     // passive — no score
    constexpr int   PTS_PER_SECOND    = 10;       // distance bonus
    constexpr int   CHAIN_BONUS[5]    = { 0, 0, 50, 100, 200 };
    //               chain:             0  1   2    3     4  (5+ = +100/ea)

    // ── Difficulty Timeline ───────────────────────────
    constexpr float SPEED_RAMP_RATE   = 0.011f;   // multiplier/sec → ×3.0 at 180s
    constexpr float SPAWN_RAMP_RATE   = 0.003f;   // spawn interval shrink/sec
    constexpr float BURNOUT_SHRINK    = 0.002f;   // zone window shrink/sec
    constexpr float INITIAL_SPAWN_INT = 1.8f;     // seconds between obstacles
    constexpr float MIN_SPAWN_INT     = 0.5f;

    // ── Rendering ─────────────────────────────────────
    constexpr float POPUP_DURATION    = 1.2f;     // score popup lifetime
    constexpr float PARTICLE_LIFE    = 0.6f;
    constexpr int   MAX_PARTICLES     = 50;

} // namespace constants
```

---

## 2. Complete Component Registry

Components are grouped by **access pattern** (hot → cold) rather than by "feature."
Hot components are iterated every frame by multiple systems. Cold components are
read infrequently or only by one system.

### 2.1 — HOT: Transform (iterated every frame by scroll, render, collision)

```cpp
// components/transform.h

/// Spatial position in logical pixels. 12 bytes.
/// Iterated by: scroll_system, render_system, collision_system,
///              burnout_system, spawn_system, cleanup_system
struct Position {
    float x;
    float y;
};

/// Movement vector in pixels/second. 8 bytes.
/// Iterated with Position by scroll_system every frame.
struct Velocity {
    float dx;
    float dy;
};
```

### 2.2 — HOT: Player State (read every frame for burnout + collision + render)

```cpp
// components/player.h

/// The three possible player forms.
enum class Shape : uint8_t {
    Circle   = 0,
    Square   = 1,
    Triangle = 2,
    Hexagon  = 3
};

/// Empty tag — marks the single player entity. 0 bytes.
struct PlayerTag {};

/// Current and transitioning shape. 4 bytes.
/// Hot: read by burnout_system, collision_system, render_system.
struct PlayerShape {
    Shape    current;      // active gameplay shape
    Shape    previous;     // for morph animation
    float    morph_t;      // 0.0 = previous, 1.0 = current (animation lerp)
};

/// Lane occupancy and transition. 8 bytes.
/// Hot: read by collision_system, player_input_system, render_system.
struct Lane {
    int8_t   current;      // 0 = left, 1 = center, 2 = right
    int8_t   target;       // where we're heading (-1 = no transition)
    float    lerp_t;       // 0.0..1.0 transition progress
};

/// Vertical movement (jump / slide / grounded). 12 bytes.
/// Hot: read by collision_system, render_system.
enum class VMode : uint8_t {
    Grounded = 0,
    Jumping  = 1,
    Sliding  = 2
};

struct VerticalState {
    VMode    mode;
    float    timer;        // time remaining in jump/slide
    float    y_offset;     // visual offset from PLAYER_Y (negative = up)
};
```

### 2.3 — HOT: Obstacle Core (iterated every frame for scroll + collision)

```cpp
// components/obstacle.h

/// Empty tag — marks all obstacle entities. 0 bytes.
struct ObstacleTag {};

/// What action this obstacle demands and its base score. 4 bytes.
/// Read by collision_system, burnout_system.
enum class ObstacleKind : uint8_t {
    ShapeGate     = 0,   // must match shape
    LaneBlock     = 1,   // legacy value kept for backward compat;
                         // spawner converts to LanePushLeft/Right at runtime
    LowBar        = 2,   // must jump
    HighBar       = 3,   // must slide
    ComboGate     = 4,   // shape + lane
    SplitPath     = 5,   // shape + specific lane
    LanePushLeft  = 6,   // passive: auto-pushes player one lane left
    LanePushRight = 7    // passive: auto-pushes player one lane right
};

struct Obstacle {
    ObstacleKind kind       = ObstacleKind::ShapeGate;
    int16_t      base_points = 200;   // PTS_SHAPE_GATE etc.
};
// Existential tag: presence means the obstacle has been cleared and awaits scoring.
struct ScoredTag {};
```

### 2.4 — WARM: Obstacle Specifics (read by collision + burnout, not by scroll)

```cpp
// components/obstacle_data.h

/// For ShapeGate / ComboGate / SplitPath: which shape is required. 1 byte.
struct RequiredShape {
    Shape shape;
};

/// For LanePushLeft/LanePushRight: push direction component. 1 byte.
/// The push direction is implicit in the ObstacleKind, so this struct
/// is no longer needed. (Legacy BlockedLanes removed with LaneBlock.)

/// For SplitPath: which lane has the opening. 1 byte.
struct RequiredLane {
    int8_t lane;    // 0, 1, or 2
};

/// For LowBar / HighBar: required vertical action. 1 byte.
struct RequiredVAction {
    VMode action;   // Jumping or Sliding
};
```

### 2.5 — HOT: Lifetime (iterated every frame for popups + particles)

```cpp
// components/lifetime.h

/// Countdown timer — entity destroyed when remaining <= 0. 8 bytes.
struct Lifetime {
    float remaining;
    float max_time;     // original duration, for lerping alpha etc.
};
```

### 2.6 — COLD: Scoring & Burnout (singletons via registry.ctx())

```cpp
// components/scoring.h

/// Singleton: overall score state. Written by scoring_system only.
struct ScoreState {
    int32_t  score;              // banked score
    int32_t  displayed_score;    // for smooth scroll-up animation
    int32_t  high_score;         // persisted across sessions
    int32_t  chain_count;        // consecutive obstacles cleared
    float    chain_timer;        // seconds since last clear — chain breaks if > threshold
    float    distance_traveled;  // total pixels scrolled (for distance bonus)
};

/// Score popup entity component. 8 bytes.
struct ScorePopup {
    int32_t  value       = 0;    // points to display
    uint8_t  tier        = 0;    // 0=normal, 1=nice, 2=great, 3=clutch, 4=insane, 5=legendary
    uint8_t  timing_tier = 255;  // TimingTier value, 255 = no timing
};
```

```cpp
// components/burnout.h

/// Burnout zone classification.
enum class BurnoutZone : uint8_t {
    None     = 0,   // no threat on screen
    Safe     = 1,   // far away — ×1.0
    Risky    = 2,   // medium  — ×1.5
    Danger   = 3,   // close   — ×3.0
    Dead     = 4    // contact — ×5.0 or crash
};

/// Singleton: burnout meter state. Recalculated each frame by burnout_system.
struct BurnoutState {
    float        meter           = 0.0f;   // 0.0..1.0 fill amount
    BurnoutZone  zone            = BurnoutZone::None;
    float        threat_distance = 0.0f;   // px to nearest unmatched obstacle
    entt::entity nearest_threat  = entt::null;  // entity id of the nearest unmatched obstacle
};
```

### 2.7 — COLD: Input (singletons, written once per frame)

```cpp
// components/input.h

/// Raw touch state — populated by input_system from raylib input. Singleton.
struct InputState {
    // Current frame touch data
    bool     touch_down;       // just pressed this frame
    bool     touch_up;         // just released this frame
    bool     touching;         // held down

    float    start_x,  start_y;    // position on touch_down
    float    curr_x,   curr_y;     // current finger position
    float    end_x,    end_y;      // position on touch_up
    float    duration;             // seconds held so far

    // Cleared each frame
    void clear_events() {
        touch_down = false;
        touch_up   = false;
    }
};

/// Classified gesture — produced by input_system, consumed by player_input_system.
enum class Gesture : uint8_t {
    None       = 0,
    Tap        = 1,
    SwipeLeft  = 2,
    SwipeRight = 3,
    SwipeUp    = 4,
    SwipeDown  = 5
};

/// Singleton: result of gesture classification.
struct GestureResult {
    Gesture  gesture;
    float    magnitude;    // swipe length in px (0 for taps)
    float    hit_x;        // where the tap/swipe started
    float    hit_y;
};

/// Singleton: shape button press (from button zone hit-test).
struct ShapeButtonEvent {
    bool     pressed;      // true if a shape button was tapped this frame
    Shape    shape;        // which shape was tapped
};
```

### 2.8 — COLD: Game State (singleton)

```cpp
// components/game_state.h

enum class GamePhase : uint8_t {
    Title    = 0,
    Playing  = 1,
    Paused   = 2,
    GameOver = 3
};

/// Singleton: governs which systems run and screen transitions.
struct GameState {
    GamePhase  phase;
    GamePhase  previous_phase;
    float      phase_timer;          // seconds in current phase
    bool       transition_pending;   // set to request a phase change
    GamePhase  next_phase;           // destination of pending transition
    float      transition_alpha;     // 0..1 for fade effects
};
```

### 2.9 — COLD: Difficulty (singleton)

```cpp
// components/difficulty.h

/// Singleton: dynamic difficulty parameters. Updated by difficulty_system.
struct DifficultyConfig {
    float  speed_multiplier;       // starts 1.0, ramps to 3.0
    float  scroll_speed;           // BASE_SCROLL_SPEED × speed_multiplier
    float  spawn_interval;         // seconds between obstacles (shrinks)
    float  spawn_timer;            // countdown to next spawn
    float  burnout_window_scale;   // 1.0 = normal, <1.0 = tighter zones
    float  elapsed;                // total seconds of gameplay (for ramp calc)
};
```

### 2.10 — COLD: Rendering Data

```cpp
// components/rendering.h

/// Per-entity color. Attached to anything visible. 4 bytes.
struct Color {
    uint8_t r, g, b, a;
};

/// Draw size in logical pixels. 8 bytes.
struct DrawSize {
    float w;
    float h;
};

/// Draw layer for z-ordering. 1 byte.
/// Render system iterates layers 0→3.
enum class Layer : uint8_t {
    Background = 0,
    Game       = 1,
    Effects    = 2,
    HUD        = 3
};

struct DrawLayer {
    Layer layer;
};
```

### 2.11 — COLD: Particles

```cpp
// components/particle.h

/// Particle-specific data (Position, Velocity, Lifetime, Color are separate). 8 bytes.
struct ParticleData {
    float  size;           // current radius in px
    float  start_size;     // initial radius (for shrink-over-life)
};

/// Empty tag — marks particle entities. 0 bytes.
struct ParticleTag {};

/// Attached to the player entity. Controls burst emission. 16 bytes.
struct ParticleEmitter {
    bool   active;
    float  emit_rate;      // particles per second
    float  accumulator;    // fractional particle accumulation
    Color  color;          // particle color for this emitter
};
```

### 2.12 — COLD: Audio (singleton)

```cpp
// components/audio.h

/// Sound effect identifiers — map to loaded Mix_Chunk* in audio init.
enum class SFX : uint8_t {
    ShapeShift     = 0,
    BurnoutBank    = 1,
    Crash          = 2,
    UITap          = 3,
    ChainBonus     = 4,
    ZoneRisky      = 5,
    ZoneDanger     = 6,
    ZoneDead       = 7,
    ScorePopup     = 8,
    GameStart      = 9,
    COUNT
};

/// Singleton: ring buffer of SFX to play this frame. Systems push, audio_system pops.
struct AudioQueue {
    static constexpr int MAX_QUEUED = 16;
    SFX      queue[MAX_QUEUED];
    int      count = 0;

    void push(SFX sfx) {
        if (count < MAX_QUEUED) {
            queue[count++] = sfx;
        }
    }

    void clear() { count = 0; }
};
```

### Component Summary Table

```
COMPONENT          BYTES   ACCESS     ITERATED BY
─────────────────────────────────────────────────────────────
Position             8     HOT        scroll, render, collision, burnout, cleanup
Velocity             8     HOT        scroll
Lifetime             8     HOT        lifetime, render (alpha fade)
PlayerTag            0     HOT        collision, burnout (filter)
PlayerShape          4     HOT        collision, burnout, render, player_action
Lane                 8     HOT        collision, render, player_action
VerticalState       12     HOT        collision, render, player_action
ObstacleTag          0     HOT        scroll, collision, burnout, cleanup
Obstacle             4     WARM       collision, burnout, scoring
RequiredShape        1     WARM       collision, burnout
BlockedLanes         1     WARM       collision
RequiredLane         1     WARM       collision
RequiredVAction      1     WARM       collision
Color                4     COLD       render
DrawSize             8     COLD       render
DrawLayer            1     COLD       render
ScorePopup           5     COLD       render, scoring
ParticleData         8     COLD       render
ParticleTag          0     COLD       render (filter)
ParticleEmitter     16     COLD       particle_spawn
─────────────────────────────────────────────────────────────
SINGLETONS (ctx)
─────────────────────────────────────────────────────────────
InputState          36     per-frame  input_system (write), player_input (read)
EventQueue          var    per-frame  input_system (write raw), hit_test_system (resolve), player_input (read), test_player_system (write)
ShapeButtonEvent     2     per-frame  input_system (write), player_action (read)
GameState           12     per-frame  game_state_system
DifficultyConfig    24     per-frame  difficulty_system (write), spawn_system (read)
BurnoutState        16     per-frame  burnout_system (write), scoring (read), render (read)
ScoreState          28     per-frame  scoring_system (write), render (read)
AudioQueue          20     per-frame  many systems (write), audio_system (read+clear)
─────────────────────────────────────────────────────────────
```

---

## 3. System Execution Order

Every system is a free function: `void name(entt::registry& reg, float dt)`.
Systems run in strict order. No system reads data written by a later system in
the same frame (unidirectional data flow).

```
 Frame N
 ═══════════════════════════════════════════════════════════════
 │
 │  ┌─ PHASE 1: INPUT CAPTURE ──────────────────────────────┐
 │  │                                                        │
 │  │  1. input_system          Read raylib input queue.  │
 │  │                           Populate InputState +        │
 │  │                           EventQueue (raw InputEvents).│
 │  │                                                        │
 │  │  2. hit_test_system       Resolve taps → ButtonPress-  │
 │  │                           Event (via HitBox/HitCircle) │
 │  │                           and swipes → GoEvent.        │
 │  └────────────────────────────────────────────────────────┘
 │
 │  ┌─ PHASE 2: GAME STATE GATE ────────────────────────────┐
 │  │                                                        │
 │  │  3. game_state_system     Process transition_pending.  │
 │  │                           On TITLE→PLAYING: spawn      │
 │  │                           player, reset singletons.    │
 │  │                           On PLAYING→GAME_OVER: save   │
 │  │                           high score, spawn crash fx.  │
 │  │                           Updates phase_timer.         │
 │  │                                                        │
 │  │  ── if phase != Playing, skip to PHASE 6 (render) ──  │
 │  └────────────────────────────────────────────────────────┘
 │
 │  ┌─ PHASE 3: PLAYER UPDATE ──────────────────────────────┐
 │  │                                                        │
 │  │  4. player_input_system   Consume EventQueue:          │
 │  │                           • ButtonPressEvent→PlayerShape│
 │  │                           • GoEvent → Lane             │
 │  │                           • jump/slide   → VertState   │
 │  │                           Push SFX::ShapeShift if      │
 │  │                           shape changed.               │
 │  │                                                        │
 │  │  5. player_movement_sys   Advance Lane.lerp_t,         │
 │  │                           VerticalState.timer,         │
 │  │                           PlayerShape.morph_t.         │
 │  │                           Update player Position.x     │
 │  │                           from lane lerp. Compute      │
 │  │                           y_offset from jump parabola. │
 │  └────────────────────────────────────────────────────────┘
 │
 │  ┌─ PHASE 4: WORLD UPDATE ───────────────────────────────┐
 │  │                                                        │
 │  │  6. difficulty_system     Advance elapsed time.        │
 │  │                           Ramp speed_multiplier,       │
 │  │                           shrink spawn_interval,       │
 │  │                           tighten burnout_window.      │
 │  │                                                        │
 │  │  7. obstacle_spawn_sys    Decrement spawn_timer.       │
 │  │                           When ≤ 0: create obstacle    │
 │  │                           entity, pick kind/lane/shape │
 │  │                           from weighted random table.  │
 │  │                           Reset timer from config.     │
 │  │                                                        │
 │  │  8. scroll_system         For every (Position, Vel):   │
 │  │                           pos.y += vel.dy * dt.        │
 │  │                           Simple, tight inner loop.    │
 │  │                                                        │
 │  │  9. burnout_system        Find nearest unmatched       │
 │  │                           obstacle. Calculate zone     │
 │  │                           from distance. Fill meter.   │
 │  │                           Write BurnoutState.          │
 │  │                           Push zone-change SFX.        │
 │  │                                                        │
 │  │ 10. collision_system      For each obstacle near       │
 │  │                           PLAYER_Y: test shape match,  │
 │  │                           lane match, vertical state.  │
 │  │                           On match: mark scored, push  │
 │  │                           SFX, trigger score event.    │
 │  │                           On mismatch at y >= player:  │
 │  │                           set transition → GAME_OVER.  │
 │  │                                                        │
 │  │ 11. scoring_system        Process scored obstacles.    │
 │  │                           Apply burnout multiplier.    │
 │  │                           Update chain. Spawn popup    │
 │  │                           entity. Add distance bonus.  │
 │  │                           Push SFX::BurnoutBank.       │
 │  └────────────────────────────────────────────────────────┘
 │
 │  ┌─ PHASE 5: CLEANUP & FX ──────────────────────────────┐
 │  │                                                        │
 │  │ 12. lifetime_system       Decrement Lifetime.remaining │
 │  │                           for popups + particles.      │
 │  │                           Destroy entity when ≤ 0.     │
 │  │                                                        │
 │  │ 13. particle_system       Advance particle positions.  │
 │  │                           Apply gravity, shrink size.  │
 │  │                           Spawn new particles from     │
 │  │                           active ParticleEmitters.     │
 │  │                                                        │
 │  │ 14. cleanup_system        Destroy obstacles with       │
 │  │                           Position.y > DESTROY_Y.      │
 │  └────────────────────────────────────────────────────────┘
 │
 │  ┌─ PHASE 6: RENDER (always runs) ──────────────────────┐
 │  │                                                        │
 │  │ 15. render_system         BeginDrawing/ClearBackground.│
 │  │                           Draw background.             │
 │  │                           Draw obstacles (Layer::Game).│
 │  │                           Draw player (Layer::Game).   │
 │  │                           Draw particles (Effects).    │
 │  │                           Draw popups (Effects).       │
 │  │                           Draw HUD (Layer::HUD):       │
 │  │                             score, speed, burnout bar, │
 │  │                             shape buttons.             │
 │  │                           EndDrawing.                  │
 │  │                                                        │
 │  │ 16. audio_system          Play all SFX in AudioQueue.  │
 │  │                           Clear queue.                 │
 │  └────────────────────────────────────────────────────────┘
 │
 ═══════════════════════════════════════════════════════════════
```

### System → Component Access Matrix

```
                        Pos  Vel  PSh  Lan  VSt  Obs  RSh  BLn  RLn  RVA  Lft  Col  DSz  DLy  SPp  PDt
 input_system            ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·
 game_state_system       ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·
 player_input_system     ·    ·    W    W    W    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·
 player_movement_sys     W    ·    W    W    W    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·
 difficulty_system        ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·
 obstacle_spawn_sys      W    W    ·    ·    ·    W    W    W    W    W    ·    W    W    W    ·    ·
 scroll_system           W    R    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·
 burnout_system          R    ·    R    R    R    R    R    R    R    R    ·    ·    ·    ·    ·    ·
 collision_system        R    ·    R    R    R    R    R    R    R    R    ·    ·    ·    ·    ·    ·
 scoring_system          W    ·    ·    ·    ·    W    ·    ·    ·    ·    ·    ·    ·    ·    W    ·
 lifetime_system         ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    W    ·    ·    ·    ·    ·
 particle_system         W    R    ·    ·    ·    ·    ·    ·    ·    ·    W    ·    ·    ·    ·    W
 cleanup_system          R    ·    ·    ·    ·    R    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·
 render_system           R    ·    R    R    R    R    R    R    ·    ·    R    R    R    R    R    R
 audio_system            ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·    ·

 R = read, W = write, · = not accessed
 Pos=Position  Vel=Velocity  PSh=PlayerShape  Lan=Lane  VSt=VerticalState
 Obs=Obstacle  RSh=RequiredShape  BLn=BlockedLanes  RLn=RequiredLane  RVA=RequiredVAction
 Lft=Lifetime  Col=Color  DSz=DrawSize  DLy=DrawLayer  SPp=ScorePopup  PDt=ParticleData
```

---

## 4. Game State Machine

```
            ┌────────────────────────────────────────────────────┐
            │                                                    │
            ▼                                                    │
     ╔═══════════╗     tap to start      ╔════════════╗         │
     ║   TITLE   ║ ────────────────────▶ ║  PLAYING   ║         │
     ╚═══════════╝                       ╚════════════╝         │
            ▲                               │      │            │
            │                    pause btn  │      │ collision  │
            │                    or app bg  │      │ mismatch   │
            │                               ▼      ▼            │
            │                         ╔══════════╗ ╔══════════╗ │
            │              resume ◀── ║  PAUSED  ║ ║ GAMEOVER ║ │
            │                         ╚══════════╝ ╚══════════╝ │
            │                               │            │      │
            │            quit to title      │    retry    │      │
            └───────────────────────────────┘            │      │
            └────────────────────────────────────────────┘      │
                                quit to title                   │
            └───────────────────────────────────────────────────┘
```

### Transition: TITLE → PLAYING

```cpp
void enter_playing(entt::registry& reg) {
    // 1. Destroy any lingering entities from previous run
    reg.clear();

    // 2. Initialize singletons
    reg.ctx().emplace<ScoreState>(ScoreState{
        .score = 0, .displayed_score = 0,
        .high_score = load_high_score(),   // from persistent storage
        .chain_count = 0, .chain_timer = 0.0f,
        .distance_traveled = 0.0f
    });
    reg.ctx().emplace<DifficultyConfig>(DifficultyConfig{
        .speed_multiplier = 1.0f,
        .scroll_speed = constants::BASE_SCROLL_SPEED,
        .spawn_interval = constants::INITIAL_SPAWN_INT,
        .spawn_timer = 1.0f,        // first obstacle after 1 sec
        .burnout_window_scale = 1.0f,
        .elapsed = 0.0f
    });
    reg.ctx().emplace<BurnoutState>();
    reg.ctx().emplace<AudioQueue>();

    // 3. Create player entity
    auto player = reg.create();
    reg.emplace<PlayerTag>(player);
    reg.emplace<Position>(player, constants::LANE_X[1], constants::PLAYER_Y);
    reg.emplace<PlayerShape>(player, Shape::Circle, Shape::Circle, 1.0f);
    reg.emplace<Lane>(player, int8_t{1}, int8_t{-1}, 1.0f);
    reg.emplace<VerticalState>(player, VMode::Grounded, 0.0f, 0.0f);
    reg.emplace<Color>(player, uint8_t{80}, uint8_t{180}, uint8_t{255}, uint8_t{255});
    reg.emplace<DrawSize>(player, constants::PLAYER_SIZE, constants::PLAYER_SIZE);
    reg.emplace<DrawLayer>(player, Layer::Game);

    // 4. Update game state
    auto& gs = reg.ctx().get<GameState>();
    gs.previous_phase = gs.phase;
    gs.phase = GamePhase::Playing;
    gs.phase_timer = 0.0f;
}
```

### Transition: PLAYING → GAME_OVER

```cpp
void enter_game_over(entt::registry& reg) {
    // 1. Persist high score
    auto& score = reg.ctx().get<ScoreState>();
    if (score.score > score.high_score) {
        score.high_score = score.score;
        save_high_score(score.high_score);   // platform-specific write
    }

    // 2. Push crash SFX
    reg.ctx().get<AudioQueue>().push(SFX::Crash);

    // 3. Spawn crash particle burst at player position
    auto view = reg.view<PlayerTag, Position>();
    for (auto [entity, pos] : view.each()) {
        spawn_particle_burst(reg, pos.x, pos.y, 30, Color{255, 80, 60, 255});
    }

    // 4. Do NOT destroy obstacles — they freeze in place for dramatic effect
    //    scroll_system will skip because phase != Playing

    // 5. Transition
    auto& gs = reg.ctx().get<GameState>();
    gs.previous_phase = gs.phase;
    gs.phase = GamePhase::GameOver;
    gs.phase_timer = 0.0f;
}
```

### Transition: GAME_OVER → PLAYING (Retry)

```cpp
void enter_retry(entt::registry& reg) {
    // Same as enter_playing — full reset
    enter_playing(reg);
}
```

### Transition: PLAYING → PAUSED

```cpp
void enter_paused(entt::registry& reg) {
    // Nothing destroyed or created — just freeze all systems
    auto& gs = reg.ctx().get<GameState>();
    gs.previous_phase = gs.phase;
    gs.phase = GamePhase::Paused;
    gs.phase_timer = 0.0f;
    // render_system draws a semi-transparent overlay + "PAUSED" text
}
```

### Transition: PAUSED → PLAYING (Resume)

```cpp
void resume_playing(entt::registry& reg) {
    auto& gs = reg.ctx().get<GameState>();
    gs.previous_phase = gs.phase;
    gs.phase = GamePhase::Playing;
    gs.phase_timer = 0.0f;
    // All entities still intact — gameplay resumes seamlessly
}
```

---

## 5. Entity Archetypes

Each "type" of entity is defined by which components it carries.
EnTT has no formal archetype — this is purely documentation of intent.

### 5.1 Player Entity (always exactly 1)

```
┌─ Player ──────────────────────────────────────────────────┐
│ PlayerTag          (tag, 0 bytes)                         │
│ Position           { x: 360.0, y: 920.0 }                │
│ PlayerShape        { current: Circle, prev: Circle,       │
│                      morph_t: 1.0 }                       │
│ Lane               { current: 1, target: -1, lerp_t: 1 } │
│ VerticalState      { mode: Grounded, timer: 0, y_off: 0 }│
│ Color              { r: 80, g: 180, b: 255, a: 255 }     │
│ DrawSize           { w: 64, h: 64 }                       │
│ DrawLayer          { layer: Game }                         │
│ ParticleEmitter    { active: false, ... }                  │
└───────────────────────────────────────────────────────────┘
Total: ~73 bytes per entity (1 entity)
```

### 5.2 Shape Gate Entity

```
┌─ Shape Gate ──────────────────────────────────────────────┐
│ ObstacleTag        (tag, 0 bytes)                         │
│ Position           { x: 360.0, y: -120.0 }               │
│ Velocity           { dx: 0.0, dy: 400.0 }                │
│ Obstacle           { kind: ShapeGate, base_pts: 200,      │
│                      scored: false }                       │
│ RequiredShape      { shape: Triangle }                     │
│ Color              { r: 50, g: 205, b: 50, a: 255 }      │
│ DrawSize           { w: 720, h: 80 }                       │
│ DrawLayer          { layer: Game }                          │
└───────────────────────────────────────────────────────────┘
Total: ~42 bytes per entity (5–15 active)
```

### 5.3 Lane Push Entity (replaces legacy Lane Block)

```
┌─ Lane Push ───────────────────────────────────────────────┐
│ ObstacleTag        (tag, 0 bytes)                         │
│ Position           { x: 360.0, y: -120.0 }               │
│ Velocity           { dx: 0.0, dy: 400.0 }                │
│ Obstacle           { kind: LanePushLeft, base_pts: 0,     │
│                      scored: false }                       │
│ Color              { r: 255, g: 60, b: 60, a: 255 }      │
│ DrawSize           { w: 720, h: 80 }                       │
│ DrawLayer          { layer: Game }                          │
└───────────────────────────────────────────────────────────┘
Total: ~40 bytes per entity

Lane Push is passive — it auto-pushes the player one lane in the
obstacle's direction on beat arrival. No player action required.
- LanePushLeft:  pushes player one lane left  (no-op on Lane 0)
- LanePushRight: pushes player one lane right (no-op on Lane 2)
- Only affects player if they are on the SAME lane as the obstacle.
- Awards 0 points (it's not a challenge).
```

### 5.4 Vertical Bar Entity (Low Bar / High Bar)

```
┌─ Vertical Bar ────────────────────────────────────────────┐
│ ObstacleTag        (tag, 0 bytes)                         │
│ Position           { x: 360.0, y: -120.0 }               │
│ Velocity           { dx: 0.0, dy: 400.0 }                │
│ Obstacle           { kind: LowBar, base_pts: 100,         │
│                      scored: false }                       │
│ RequiredVAction    { action: Jumping }                     │
│ Color              { r: 255, g: 180, b: 0, a: 255 }      │
│ DrawSize           { w: 720, h: 40 }                       │
│ DrawLayer          { layer: Game }                          │
└───────────────────────────────────────────────────────────┘
Total: ~41 bytes per entity
```

### 5.5 Combo Gate Entity (Shape + Lane)

```
┌─ Combo Gate ──────────────────────────────────────────────┐
│ ObstacleTag        (tag, 0 bytes)                         │
│ Position           { x: 360.0, y: -120.0 }               │
│ Velocity           { dx: 0.0, dy: 400.0 }                │
│ Obstacle           { kind: ComboGate, base_pts: 200,      │
│                      scored: false }                       │
│ RequiredShape      { shape: Square }                       │
│ BlockedLanes       { mask: 0b110 }                         │
│ Color              { r: 200, g: 100, b: 255, a: 255 }    │
│ DrawSize           { w: 720, h: 80 }                       │
│ DrawLayer          { layer: Game }                          │
└───────────────────────────────────────────────────────────┘
Total: ~43 bytes per entity
```

### 5.6 Split Path Entity (Shape + Specific Lane)

```
┌─ Split Path ──────────────────────────────────────────────┐
│ ObstacleTag        (tag, 0 bytes)                         │
│ Position           { x: 360.0, y: -120.0 }               │
│ Velocity           { dx: 0.0, dy: 400.0 }                │
│ Obstacle           { kind: SplitPath, base_pts: 300,      │
│                      scored: false }                       │
│ RequiredShape      { shape: Circle }                       │
│ RequiredLane       { lane: 2 }                             │
│ Color              { r: 255, g: 215, b: 0, a: 255 }      │
│ DrawSize           { w: 720, h: 80 }                       │
│ DrawLayer          { layer: Game }                          │
└───────────────────────────────────────────────────────────┘
Total: ~44 bytes per entity
```

### 5.7 Score Popup Entity

```
┌─ Score Popup ─────────────────────────────────────────────┐
│ Position           { x: 360.0, y: 900.0 }                │
│ Velocity           { dx: 0.0, dy: -80.0 }  (floats up)   │
│ Lifetime           { remaining: 1.2, max_time: 1.2 }      │
│ ScorePopup         { value: 600, tier: 3, timing_tier: 255 }  │
│ Color              { r: 255, g: 200, b: 50, a: 255 }     │
│ DrawLayer          { layer: Effects }                      │
└───────────────────────────────────────────────────────────┘
Total: ~33 bytes per entity (0–5 active)
```

### 5.8 Particle Entity

```
┌─ Particle ────────────────────────────────────────────────┐
│ ParticleTag        (tag, 0 bytes)                         │
│ Position           { x: 360.0, y: 920.0 }                │
│ Velocity           { dx: rand, dy: rand }  (random burst) │
│ Lifetime           { remaining: 0.6, max_time: 0.6 }      │
│ ParticleData       { size: 4.0, start_size: 4.0 }         │
│ Color              { r: 255, g: 100, b: 50, a: 255 }     │
│ DrawLayer          { layer: Effects }                      │
└───────────────────────────────────────────────────────────┘
Total: ~37 bytes per entity (0–50 active)
```

---

## 6. Main Loop Structure

### 6.1 Timestep Decision: Semi-Fixed with Accumulator

Mobile devices run at 60Hz, but thermal throttling, OS interrupts, and
variable refresh rates mean we cannot assume a stable frame time.

**Strategy**: fixed-step logic at 60 Hz, variable render with interpolation.
Cap accumulator to prevent spiral of death after app-resume pauses.

```cpp
// main.cpp — complete main loop pseudocode

#include <raylib.h>
#include <entt/entt.hpp>
#include "constants.h"
#include "systems/all_systems.h"

int main(int argc, char* argv[]) {

    // ── RAYLIB INIT ──────────────────────────────────────────────
    InitWindow(constants::SCREEN_W, constants::SCREEN_H, "SHAPESHIFTER");
    SetTargetFPS(60);
    InitAudioDevice();

    // ── ENTT REGISTRY ─────────────────────────────────────────
    entt::registry reg;

    // Emplace all singletons with defaults
    reg.ctx().emplace<InputState>();
    reg.ctx().emplace<GestureResult>();
    reg.ctx().emplace<ShapeButtonEvent>();
    reg.ctx().emplace<GameState>(GameState{
        .phase = GamePhase::Title,
        .previous_phase = GamePhase::Title,
        .phase_timer = 0.0f,
        .transition_pending = false,
        .next_phase = GamePhase::Title,
        .transition_alpha = 0.0f
    });
    reg.ctx().emplace<AudioQueue>();

    // Load audio assets, textures, fonts into ctx()...
    load_audio_assets(reg);

    // ── TIMING ────────────────────────────────────────────────
    constexpr float FIXED_DT     = 1.0f / 60.0f;   // 16.67ms
    constexpr float MAX_ACCUM    = 0.1f;            // cap = 6 frames
    float           accumulator  = 0.0f;
    double          prev_time    = GetTime();

    // ── MAIN LOOP ─────────────────────────────────────────────
    while (!WindowShouldClose()) {

        // ── DELTA TIME ────────────────────────────────────────
        double now = GetTime();
        float raw_dt = static_cast<float>(now - prev_time);
        prev_time = now;
        accumulator += raw_dt;
        if (accumulator > MAX_ACCUM) {
            accumulator = MAX_ACCUM;   // prevent spiral of death
        }

        // ── INPUT (once per frame, outside fixed loop) ──
        input_system(reg, raw_dt);

        // ── FIXED TIMESTEP LOOP ──────────────────────────────
        while (accumulator >= FIXED_DT) {

            //  Phase 1: Input Classification
            // (gesture_system removed; input classified in input_system)

            //  Phase 2: Game State Gate
            game_state_system(reg, FIXED_DT);

            auto phase = reg.ctx().get<GameState>().phase;

            if (phase == GamePhase::Playing) {
                //  Phase 3: Player
                player_input_system(reg, FIXED_DT);
                player_movement_system(reg, FIXED_DT);

                //  Phase 4: World
                difficulty_system(reg, FIXED_DT);
                obstacle_spawn_system(reg, FIXED_DT);
                scroll_system(reg, FIXED_DT);
                burnout_system(reg, FIXED_DT);
                collision_system(reg, FIXED_DT);
                scoring_system(reg, FIXED_DT);
            }

            //  Phase 5: Cleanup (runs in all phases to drain popups/particles)
            lifetime_system(reg, FIXED_DT);
            particle_system(reg, FIXED_DT);
            cleanup_system(reg, FIXED_DT);

            accumulator -= FIXED_DT;
        }

        // ── RENDER (once per frame, variable rate) ────────────
        float alpha = accumulator / FIXED_DT;   // interpolation factor
        render_system(reg, alpha);

        // ── AUDIO (once per frame, after render) ──────────────
        audio_system(reg);
    }

    // ── SHUTDOWN ──────────────────────────────────────────────
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
```

### 6.2 Fixed vs Variable Breakdown

```
┌────────────────────────────────┬──────────┬──────────┐
│ Operation                      │ Timestep │ Why?     │
├────────────────────────────────┼──────────┼──────────┤
│ raylib input polling            │ Variable │ OS events│
│ game_state_system              │ Fixed    │ Logic    │
│ player_input_system            │ Fixed    │ Logic    │
│ player_movement_system         │ Fixed    │ Physics  │
│ difficulty_system              │ Fixed    │ Logic    │
│ obstacle_spawn_system          │ Fixed    │ Logic    │
│ scroll_system                  │ Fixed    │ Physics  │
│ burnout_system                 │ Fixed    │ Logic    │
│ collision_system               │ Fixed    │ Physics  │
│ scoring_system                 │ Fixed    │ Logic    │
│ lifetime_system                │ Fixed    │ Timing   │
│ particle_system                │ Fixed    │ Physics  │
│ cleanup_system                 │ Fixed    │ Logic    │
│ render_system                  │ Variable │ Display  │
│ audio_system                   │ Variable │ Playback │
└────────────────────────────────┴──────────┴──────────┘
```

---

## 7. Data Flow Diagrams

### 7.1 Critical Path: Obstacle Approach → Burnout → Score

```
    OBSTACLE ENTITY                     SINGLETONS              PLAYER ENTITY
    ┌──────────────┐                                            ┌─────────────┐
    │  Position.y  │                                            │ PlayerShape │
    │  Velocity.dy │                                            │ Lane        │
    │  Obstacle    │                                            │ VertState   │
    │  RequiredShape│                                           │ Position    │
    └──────┬───────┘                                            └──────┬──────┘
           │                                                           │
           │  scroll_system: pos.y += vel.dy × dt                      │
           │                                                           │
           ▼                                                           │
    ┌──────────────┐     burnout_system:                                │
    │ obstacle     │     for each obstacle near player,                 │
    │ Position.y   │     check if player shape matches                  │
    │ (updated)    │─────RequiredShape →─────────────────────┐          │
    └──────┬───────┘                                        │          │
           │                                                ▼          ▼
           │                                        ┌──────────────────────┐
           │                                        │ burnout_system:      │
           │                                        │   distance =         │
           │                                        │     player.y -       │
           │                                        │     obstacle.y       │
           │                                        │   if shapes don't    │
           │                                        │   match → calc zone  │
           │                                        │   map to 0..1 meter  │
           │                                        └──────────┬───────────┘
           │                                                   │
           │                                                   ▼
           │                                        ┌──────────────────────┐
           │                                        │ BurnoutState (ctx)   │
           │                                        │   meter: 0.73        │
           │                                        │   zone: Danger       │
           │                                        │   threat_dist: 185px │
           │                                        │   nearest_threat: e4 │
           │                                        └──────────┬───────────┘
           │                                                   │
           │     PLAYER TAPS SHAPE BUTTON                      │
           │     ┌───────────────────────┐                     │
           │     │ ShapeButtonEvent (ctx)│                     │
           │     │   pressed: true       │                     │
           │     │   shape: Triangle     │                     │
           │     └──────────┬────────────┘                     │
           │                │                                  │
           │                ▼                                  │
           │     ┌───────────────────────┐                     │
           │     │ player_input_system:  │                     │
           │     │   PlayerShape.current │                     │
           │     │   = Triangle          │                     │
           │     │   morph_t = 0.0       │                     │
           │     └──────────┬────────────┘                     │
           │                │                                  │
           │                │   NEXT FRAME                     │
           │                ▼                                  │
           │     ┌───────────────────────────────────┐         │
           │     │ collision_system:                  │         │
           │     │   obstacle reaches player Y        │         │
           │     │   shape matches → emplace ScoredTag│         │
           │     └──────────┬────────────────────────┘         │
           │                │                                  │
           │                ▼                                  │
           │     ┌───────────────────────────────────────────┐ │
           │     │ scoring_system:                            │ │
           │     │   reads BurnoutState.zone at time of match│◀┘
           │     │   multiplier = zone_to_mult(Danger) = 3.0 │
           │     │   chain_bonus = chain_to_bonus(chain=3)   │
           │     │              = +100                        │
           │     │   total = 200 × 3.0 + 100 = 700           │
           │     │   ScoreState.score += 700                  │
           │     │   spawn ScorePopup(700, tier=3)            │
           │     │   AudioQueue.push(BurnoutBank)             │
           │     └───────────────────────────────────────────┘
           │
           ▼
    ┌──────────────┐
    │ cleanup_sys: │
    │  pos.y >     │
    │  DESTROY_Y → │
    │  destroy     │
    └──────────────┘
```

### 7.2 Critical Path: Touch → Gesture → Player Action

```
    RAYLIB INPUT                        SINGLETONS                    PLAYER ENTITY
    ┌─────────────┐
    │ Touch        │
    │   DOWN       │
    │  x=0.52      │    input_system (reads raylib input):
    │  y=0.91      │    convert normalized → logical coords
    │              │    x = 0.52 × 720 = 374
    └──────┬──────┘    y = 0.91 × 1280 = 1165
           │
           │           ┌──────────────────────────────────────┐
           │           │ HIT TEST: y > 1020 (button zone)?   │
           ├──────────▶│ YES → check which shape button      │
           │           │   x ∈ [120,240] → Circle            │
           │           │   x ∈ [300,420] → Square            │
           │           │   x ∈ [480,600] → Triangle          │
           │           │   374 ∈ [300,420] → Square          │
           │           └──────────────────┬───────────────────┘
           │                              │
           │                              ▼
           │           ┌──────────────────────────────────────┐
           │           │ ShapeButtonEvent (ctx)               │
           │           │   .pressed = true                    │
           │           │   .shape   = Square                  │
           │           └──────────────────┬───────────────────┘
           │                              │
           │                              │
    ┌──────┴──────┐                       │
    │ Touch        │                       │
    │   DOWN       │                       │
    │  x=0.20      │    input_system:      │
    │  y=0.40      │    y=512, < 1020      │
    └──────┬──────┘    → swipe zone        │
           │           populate InputState │
           │                              │
           ▼                              │
    ┌─────────────┐                       │
    │ Touch        │                       │
    │   MOTION     │                       │
    │  x=0.05      │                       │
    │  y=0.42      │                       │
    └──────┬──────┘                       │
           │                              │
           ▼                              │
    ┌─────────────┐                       │
    │ Touch        │                       │
    │   UP         │                       │
    │  x=0.03      │                       │
    │  y=0.43      │                       │
    └──────┬──────┘                       │
           │                              │
           ▼                              │
    ┌──────────────────────────┐          │
    │ input_system:            │          │
    │   dx = end_x - start_x  │          │
    │      = 22 - 144 = -122  │          │
    │   dy = end_y - start_y  │          │
    │      = 550 - 512 = 38   │          │
    │   |dx| > |dy|           │          │
    │   |dx| > SWIPE_THRESH   │          │
    │   dx < 0 → SwipeLeft    │          │
    │   duration < 0.3s → ok  │          │
    └──────────┬───────────────┘          │
               │                          │
               ▼                          │
    ┌──────────────────────────┐          │
    │ EventQueue (ctx)         │          │
    │   .goes[0] = { Left }   │          │
    │   .go_count = 1          │          │
    └──────────┬───────────────┘          │
               │                          │
               ▼                          ▼
    ┌────────────────────────────────────────────────────────┐
    │ player_input_system:                                   │
    │                                                        │
    │   // 1. Process shape button                           │
    │   if (ShapeButtonEvent.pressed) {                      │
    │       PlayerShape.previous = PlayerShape.current;      │──▶ PlayerShape
    │       PlayerShape.current  = ShapeButtonEvent.shape;   │    { current: Square,
    │       PlayerShape.morph_t  = 0.0f;                     │      previous: Circle,
    │       AudioQueue.push(ShapeShift);                     │      morph_t: 0.0 }
    │   }                                                    │
    │                                                        │
    │   // 2. Process gesture                                │
    │   switch (GestureResult.gesture) {                     │
    │       case SwipeLeft:                                   │
    │           if (Lane.current > 0) {                      │──▶ Lane
    │               Lane.target = Lane.current - 1;          │    { current: 1,
    │               Lane.lerp_t = 0.0f;                      │      target: 0,
    │           }                                            │      lerp_t: 0.0 }
    │           break;                                       │
    │       case SwipeUp:                                     │
    │           if (VertState.mode == Grounded) {             │──▶ VerticalState
    │               VertState.mode  = Jumping;               │    { mode: Jumping,
    │               VertState.timer = JUMP_DURATION;         │      timer: 0.45,
    │           }                                            │      y_offset: 0.0 }
    │           break;                                       │
    │       // ... SwipeRight, SwipeDown ...                  │
    │   }                                                    │
    └────────────────────────────────────────────────────────┘
```

### 7.3 Burnout Meter Calculation (detail)

```
    burnout_system(reg, dt):

    player_pos  ◀── view<PlayerTag, Position, PlayerShape, Lane>
    player_shape ◀┘

    nearest_dist = FLOAT_MAX
    nearest_ent  = entt::null

    ┌───────── for each (entity, pos, obs, ...) in view<ObstacleTag, Position, Obstacle> ─┐
    │                                                                                       │
    │   if (obs.scored) continue;                   // already banked                       │
    │   if (pos.y > player_pos.y) continue;         // behind player                       │
    │                                                                                       │
    │   dist = player_pos.y - pos.y                                                         │
    │                                                                                       │
    │   // Is this obstacle a threat? (does player need to act?)                            │
    │   bool threat = false;                                                                │
    │   if (obs.kind == ShapeGate && has<RequiredShape>(entity))                             │
    │       threat = (player_shape.current != get<RequiredShape>(entity).shape);             │
    │   else if (obs.kind == LanePushLeft || obs.kind == LanePushRight)                       │
    │       threat = false;   // passive — never a threat, auto-pushes player on arrival     │
    │   else if (obs.kind == LowBar)                                                        │
    │       threat = (player_vstate.mode != Jumping);                                       │
    │   // ... etc for each ObstacleKind ...                                                │
    │                                                                                       │
    │   if (threat && dist < nearest_dist) {                                                │
    │       nearest_dist = dist;                                                            │
    │       nearest_ent  = entity;                                                          │
    │   }                                                                                   │
    └───────────────────────────────────────────────────────────────────────────────────────┘

    // Map distance to zone and meter
    BurnoutZone zone;
    float meter;
    float window = burnout_window_scale;   // from DifficultyConfig

    if (nearest_dist > ZONE_SAFE_MAX × window)
        zone = None,    meter = 0.0f;
    else if (nearest_dist > ZONE_SAFE_MIN × window)
        zone = Safe,    meter = remap(nearest_dist, SAFE_MAX, SAFE_MIN, 0.0, 0.25);
    else if (nearest_dist > ZONE_RISKY_MIN × window)
        zone = Risky,   meter = remap(nearest_dist, SAFE_MIN, RISKY_MIN, 0.25, 0.55);
    else if (nearest_dist > ZONE_DANGER_MIN × window)
        zone = Danger,  meter = remap(nearest_dist, RISKY_MIN, DANGER_MIN, 0.55, 0.85);
    else
        zone = Dead,    meter = remap(nearest_dist, DANGER_MIN, 0, 0.85, 1.0);

    // Write singleton
    auto& bs = reg.ctx().get<BurnoutState>();
    BurnoutZone old_zone = bs.zone;
    bs.meter           = meter;
    bs.zone            = zone;
    bs.threat_distance = nearest_dist;
    bs.nearest_threat  = nearest_ent;
    bs.has_threat      = (nearest_ent != entt::null);

    // SFX on zone transitions
    if (zone != old_zone && zone > old_zone) {
        if      (zone == Risky)  reg.ctx().get<AudioQueue>().push(SFX::ZoneRisky);
        else if (zone == Danger) reg.ctx().get<AudioQueue>().push(SFX::ZoneDanger);
        else if (zone == Dead)   reg.ctx().get<AudioQueue>().push(SFX::ZoneDead);
    }
```

---

## 8. Memory & Performance Considerations

### 8.1 Entity Count Budget

```
┌──────────────────┬──────────┬──────────────┬──────────────┐
│ Entity Type      │ Count    │ Bytes/Entity │ Total Bytes  │
├──────────────────┼──────────┼──────────────┼──────────────┤
│ Player           │ 1        │ ~73          │ 73           │
│ Obstacles        │ 5–15     │ ~42          │ 210–630      │
│ Score Popups     │ 0–5      │ ~33          │ 0–165        │
│ Particles        │ 0–50     │ ~37          │ 0–1850       │
├──────────────────┼──────────┼──────────────┼──────────────┤
│ TOTAL (peak)     │ ~71      │              │ ~2,718       │
│ + Singletons     │          │              │ ~130         │
│ + EnTT overhead  │          │              │ ~8,000       │
├──────────────────┼──────────┼──────────────┼──────────────┤
│ GRAND TOTAL      │          │              │ ~11 KB       │
└──────────────────┴──────────┴──────────────┴──────────────┘

This entire game state fits in L1 cache (~32-64 KB).
```

### 8.2 Hot / Cold Component Classification

```
  ACCESS FREQUENCY
  ▲
  │
  │  ■ Position        (every system, every frame, R+W)
  │  ■ Velocity        (scroll + particle, every frame, R+W)
  │  ■ Lifetime        (lifetime sys, every frame, R+W)
  │
  │  □ PlayerShape     (burnout + collision + render, every frame, R)
  │  □ Lane            (burnout + collision + render, every frame, R)
  │  □ VerticalState   (collision + render, every frame, R)
  │  □ Obstacle        (burnout + collision, every frame, R)
  │
  │  ○ RequiredShape   (collision + burnout, per-obstacle, R)
  │  ○ BlockedLanes    (collision, per-obstacle, R)
  │  ○ RequiredVAction (collision, per-obstacle, R)
  │  ○ Color           (render only, R)
  │  ○ DrawSize        (render only, R)
  │
  │  · ParticleData    (particle sys only, R+W)
  │  · ScorePopup      (render only, R)
  │  · ParticleEmitter (particle spawn only, R+W)
  │
  └──────────────────────────────────────────────▶  ENTITY COUNT
     1 (player)          15 (obstacles)     50 (particles)
```

### 8.3 EnTT Storage Model: Why AoS is Fine Here

EnTT stores each component type in its own **sparse set** — effectively a
`std::vector<T>` per component type with an indirection layer. This gives us:

```
Position pool:   [pos0] [pos1] [pos2] [pos3] ... [posN]   contiguous in memory
Velocity pool:   [vel0] [vel1] [vel2] [vel3] ... [velN]   contiguous in memory
Obstacle pool:   [obs0] [obs1] [obs2] ... [obsM]          contiguous in memory
```

**For SHAPESHIFTER, AoS (EnTT default) is optimal because:**

1. **Entity counts are tiny** (~71 peak). The entire Position pool fits in a
   single cache line at 8 bytes × 71 = 568 bytes. There is no cache pressure
   that SoA would alleviate.

2. **Iteration patterns are simple**. The hottest loop (`scroll_system`) reads
   Position + Velocity together — two pools that are each < 1 KB. Both will
   be in L1 after the first iteration.

3. **SoA (splitting Position into x[] and y[])** would add indirection overhead
   and code complexity for zero performance gain at these entity counts. SoA
   becomes beneficial at 10,000+ entities — we peak at 71.

**EnTT groups** can ensure co-iterated component pools are sorted identically:

```cpp
// In initialization — declare a full-owning group for Position + Velocity.
// This guarantees both pools are sorted in the same entity order,
// so iterating the group produces perfectly sequential memory access.
auto& group = reg.group<Position, Velocity>();

// scroll_system uses this group:
void scroll_system(entt::registry& reg, float dt) {
    auto group = reg.group<Position, Velocity>();
    for (auto [entity, pos, vel] : group.each()) {
        pos.x += vel.dx * dt;
        pos.y += vel.dy * dt;
    }
}
```

### 8.4 CPU Budget per Frame (Target: < 2ms logic at 60 Hz on mobile)

```
┌───────────────────────┬───────────────┬───────────────────────────┐
│ System                │ Est. Time     │ Rationale                 │
├───────────────────────┼───────────────┼───────────────────────────┤
│ input + gesture       │ < 0.01 ms     │ Singleton writes only     │
│ game_state            │ < 0.01 ms     │ Branch + singleton write  │
│ player_action         │ < 0.01 ms     │ 1 entity                 │
│ player_movement       │ < 0.01 ms     │ 1 entity                 │
│ difficulty            │ < 0.01 ms     │ Singleton math            │
│ obstacle_spawn        │ < 0.05 ms     │ 0-1 entity create/frame   │
│ scroll_system         │ < 0.05 ms     │ ~71 entities, 2 floats    │
│ burnout_system        │ < 0.10 ms     │ ~15 obstacles × player    │
│ collision_system      │ < 0.10 ms     │ ~15 obstacles × player    │
│ scoring_system        │ < 0.05 ms     │ 0-1 scores/frame          │
│ lifetime_system       │ < 0.05 ms     │ ~55 entities              │
│ particle_system       │ < 0.10 ms     │ ~50 particles             │
│ cleanup_system        │ < 0.05 ms     │ scan + destroy            │
│ render_system         │ < 1.50 ms     │ raylib draw calls (GPU)   │
│ audio_system          │ < 0.05 ms     │ 0-3 PlaySound calls       │
├───────────────────────┼───────────────┼───────────────────────────┤
│ TOTAL                 │ < 2.2 ms      │ Well within 16.67ms budget│
└───────────────────────┴───────────────┴───────────────────────────┘
```

### 8.5 Collision Detection Strategy

With only 5–15 obstacles on screen and 1 player, brute-force is optimal:

```cpp
void collision_system(entt::registry& reg, float dt) {
    // Get player state (single entity)
    auto player_view = reg.view<PlayerTag, Position, PlayerShape, Lane, VerticalState>();
    // Early-out: exactly one player
    auto [p_ent, p_pos, p_shape, p_lane, p_vstate] = *player_view.each().begin();

    float player_x = p_pos.x;
    float player_y = p_pos.y + p_vstate.y_offset;

    // Linear scan of obstacles — 15 entities max
    auto obs_view = reg.view<ObstacleTag, Position, Obstacle>();
    for (auto [entity, o_pos, obs] : obs_view.each()) {
        if (obs.scored) continue;

        // Broad phase: vertical proximity check
        float dy = o_pos.y - player_y;
        if (dy < -40.0f || dy > 40.0f) continue;  // not overlapping vertically

        // Narrow phase: per-obstacle-kind match test
        bool cleared = check_obstacle_cleared(reg, entity, obs,
                                               p_shape, p_lane, p_vstate);
        if (cleared) {
            obs.scored = true;
            // scoring_system will pick this up next
        } else if (dy >= 0.0f) {
            // Obstacle has reached player and shape doesn't match → CRASH
            auto& gs = reg.ctx().get<GameState>();
            gs.transition_pending = true;
            gs.next_phase = GamePhase::GameOver;
            return;  // stop checking — game over
        }
    }
}

// Per-kind match logic — no virtual dispatch, just a switch
bool check_obstacle_cleared(entt::registry& reg, entt::entity e,
                             const Obstacle& obs,
                             const PlayerShape& shape,
                             const Lane& lane,
                             const VerticalState& vs) {
    switch (obs.kind) {
        case ObstacleKind::ShapeGate:
            return shape.current == reg.get<RequiredShape>(e).shape;

        case ObstacleKind::LanePushLeft:
        case ObstacleKind::LanePushRight:
            return true;   // passive — always "cleared", push applied automatically

        case ObstacleKind::LowBar:
            return vs.mode == VMode::Jumping;

        case ObstacleKind::HighBar:
            return vs.mode == VMode::Sliding;

        case ObstacleKind::ComboGate: {
            bool shape_ok = shape.current == reg.get<RequiredShape>(e).shape;
            uint8_t mask  = reg.get<BlockedLanes>(e).mask;
            bool lane_ok  = !((mask >> lane.current) & 1);
            return shape_ok && lane_ok;
        }

        case ObstacleKind::SplitPath: {
            bool shape_ok = shape.current == reg.get<RequiredShape>(e).shape;
            bool lane_ok  = lane.current == reg.get<RequiredLane>(e).lane;
            return shape_ok && lane_ok;
        }
    }
    return false;
}
```

No spatial hash. No quadtree. At 15 entities, the overhead of any acceleration
structure exceeds the cost of the brute-force scan. If the game ever needed
100+ simultaneous obstacles, add a **Y-sorted list** and binary-search for the
collision window — but that day will not come for an endless runner.

### 8.6 Render System Organization

The render system draws in layer order to avoid sorting:

```cpp
void render_system(entt::registry& reg, float alpha) {
    ClearBackground({18, 18, 24, 255});

    auto phase = reg.ctx().get<GameState>().phase;

    // ── Layer 0: Background ───────────────────────────
    draw_background();
    draw_lane_lines();

    // ── Layer 1: Game entities ────────────────────────
    if (phase == GamePhase::Playing || phase == GamePhase::Paused
        || phase == GamePhase::GameOver) {

        // Obstacles (5-15 entities — draw in pool order, no sort needed)
        {
            auto view = reg.view<ObstacleTag, Position, Obstacle, Color, DrawSize>();
            for (auto [e, pos, obs, col, sz] : view.each()) {
                draw_obstacle(pos, obs, col, sz, reg, e);
            }
        }

        // Player (1 entity)
        {
            auto view = reg.view<PlayerTag, Position, PlayerShape,
                                  Lane, VerticalState, Color, DrawSize>();
            for (auto [e, pos, shape, lane, vs, col, sz] : view.each()) {
                // Interpolate position for smooth sub-frame rendering
                float render_x = pos.x;  // lane lerp already applied
                float render_y = pos.y + vs.y_offset;
                draw_player_shape(render_x, render_y,
                                  shape, col, sz);
            }
        }
    }

    // ── Layer 2: Effects ──────────────────────────────
    // Particles
    {
        auto view = reg.view<ParticleTag, Position, ParticleData, Color, Lifetime>();
        for (auto [e, pos, pd, col, life] : view.each()) {
            float t = life.remaining / life.max_time;
            uint8_t a = static_cast<uint8_t>(col.a * t);
            float size = pd.size * t;
            draw_particle(pos.x, pos.y, size, {col.r, col.g, col.b, a});
        }
    }

    // Score popups
    {
        auto view = reg.view<ScorePopup, Position, Lifetime>();
        for (auto [e, popup, pos, life] : view.each()) {
            float t = life.remaining / life.max_time;
            draw_score_popup(pos.x, pos.y, popup.value, popup.tier, t);
        }
    }

    // ── Layer 3: HUD ──────────────────────────────────
    if (phase == GamePhase::Playing || phase == GamePhase::Paused) {
        auto& score  = reg.ctx().get<ScoreState>();
        auto& burnout = reg.ctx().get<BurnoutState>();
        draw_hud_score(score);
        draw_burnout_meter(burnout);
        draw_shape_buttons(reg);
    }

    // ── Overlays ──────────────────────────────────────
    if (phase == GamePhase::Title) {
        draw_title_screen(reg);
    } else if (phase == GamePhase::Paused) {
        draw_pause_overlay();
    } else if (phase == GamePhase::GameOver) {
        auto& gs = reg.ctx().get<GameState>();
        draw_game_over(reg, gs.phase_timer);
    }

    EndDrawing();
}
```

### 8.7 File / Module Organization

```
app/
├── CMakeLists.txt
├── main.cpp                     ← main loop (§6)
├── constants.h                  ← all tuning knobs (§1)
│
├── components/                  ← all component structs
│   ├── transform.h              ← Position, Velocity
│   ├── player.h                 ← PlayerTag, PlayerShape, Lane, VerticalState
│   ├── obstacle.h               ← ObstacleTag, Obstacle, ObstacleKind
│   ├── obstacle_data.h          ← RequiredShape, BlockedLanes, RequiredLane, RequiredVAction
│   ├── scoring.h                ← ScoreState, ScorePopup
│   ├── burnout.h                ← BurnoutState, BurnoutZone
│   ├── input.h                  ← InputState, GestureResult, ShapeButtonEvent
│   ├── game_state.h             ← GameState, GamePhase
│   ├── difficulty.h             ← DifficultyConfig
│   ├── rendering.h              ← Color, DrawSize, DrawLayer, Layer
│   ├── particle.h               ← ParticleData, ParticleTag, ParticleEmitter
│   ├── lifetime.h               ← Lifetime
│   └── audio.h                  ← SFX, AudioQueue
│
├── systems/                     ← all system free functions
│   ├── all_systems.h            ← convenience #include for all systems
│   ├── input_system.cpp         ← raylib input → InputState + EventQueue (raw InputEvents)
│   ├── hit_test_system.cpp     ← resolves taps → ButtonPressEvent, swipes → GoEvent
│   ├── game_state_system.cpp    ← phase transitions
│   ├── player_input_system.cpp  ← EventQueue (ButtonPressEvent + GoEvent) → player component writes
│   ├── test_player_system.cpp   ← automated test player (writes EventQueue)
│   ├── player_movement_system.cpp ← lane lerp, jump parabola, morph advance
│   ├── difficulty_system.cpp    ← ramp speed, spawn interval, burnout window
│   ├── obstacle_spawn_system.cpp ← create obstacle entities
│   ├── scroll_system.cpp        ← pos += vel × dt
│   ├── burnout_system.cpp       ← nearest threat → meter + zone
│   ├── collision_system.cpp     ← obstacle vs player match test
│   ├── scoring_system.cpp       ← bank points, chain, spawn popup
│   ├── lifetime_system.cpp      ← countdown, destroy
│   ├── particle_system.cpp      ← advance, spawn, cull
│   ├── cleanup_system.cpp       ← off-screen entity removal
│   ├── render_system.cpp        ← raylib draw calls
│   └── audio_system.cpp         ← AudioQueue → PlaySound
│
└── util/
    ├── math_util.h              ← remap(), lerp(), clamp()
    ├── random.h                 ← seeded RNG (xoshiro256**)
    └── persistence.h            ← load/save high score (platform-specific)
```

---

## Appendix A: EnTT Patterns Used

### A.1 Singleton Access

```cpp
// Write (system that produces data):
auto& burnout = reg.ctx().get<BurnoutState>();
burnout.meter = calculated_value;

// Read (system that consumes data):
const auto& burnout = reg.ctx().get<const BurnoutState>();
float m = burnout.meter;
```

### A.2 View Iteration (most common)

```cpp
// Multi-component view — O(n) where n = smallest pool
auto view = reg.view<ObstacleTag, Position, Obstacle>();
for (auto [entity, pos, obs] : view.each()) {
    // ...
}
```

### A.3 Group for Hot Path

```cpp
// Declared once at startup — ensures Position and Velocity pools
// maintain identical sort order for cache-optimal co-iteration.
reg.group<Position, Velocity>();

// Used in scroll_system:
auto group = reg.group<Position, Velocity>();
for (auto [entity, pos, vel] : group.each()) {
    pos.y += vel.dy * dt;
}
```

### A.4 Entity Creation (obstacle spawn)

```cpp
entt::entity spawn_shape_gate(entt::registry& reg, Shape required,
                               int8_t lane, float scroll_speed) {
    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);
    reg.emplace<Position>(e, constants::LANE_X[lane], constants::SPAWN_Y);
    reg.emplace<Velocity>(e, 0.0f, scroll_speed);
    reg.emplace<Obstacle>(e, ObstacleKind::ShapeGate,
                          static_cast<int16_t>(constants::PTS_SHAPE_GATE));
    reg.emplace<RequiredShape>(e, required);
    reg.emplace<Color>(e, shape_color(required));
    reg.emplace<DrawSize>(e, static_cast<float>(constants::SCREEN_W), 80.0f);
    reg.emplace<DrawLayer>(e, Layer::Game);
    return e;
}
```

### A.5 Entity Destruction

```cpp
// cleanup_system: destroy off-screen obstacles
auto view = reg.view<ObstacleTag, Position>();
for (auto [entity, pos] : view.each()) {
    if (pos.y > constants::DESTROY_Y) {
        reg.destroy(entity);   // safe during iteration in EnTT v3
    }
}

// lifetime_system: destroy expired popups/particles
auto view = reg.view<Lifetime>();
for (auto [entity, life] : view.each()) {
    life.remaining -= dt;
    if (life.remaining <= 0.0f) {
        reg.destroy(entity);
    }
}
```

---

## Appendix B: Shape Color Table

```cpp
// Used by obstacle spawn and player render to ensure visual consistency.

constexpr Color SHAPE_COLORS[3] = {
    { 80, 180, 255, 255 },   // Circle   — sky blue
    { 255,  80,  80, 255 },  // Square   — coral red
    {  50, 220, 100, 255 }   // Triangle — emerald green
};

constexpr Color shape_color(Shape s) {
    return SHAPE_COLORS[static_cast<uint8_t>(s)];
}

constexpr Color BURNOUT_ZONE_COLORS[5] = {
    {  80,  80,  80, 255 },  // None     — dark gray
    {  80, 200,  80, 255 },  // Safe     — green
    { 255, 220,  50, 255 },  // Risky    — yellow
    { 255, 120,  30, 255 },  // Danger   — orange
    { 255,  40,  40, 255 }   // Dead     — red
};
```

---

*This document defines the complete data layout. No component holds a pointer to
another component. No system calls a virtual method. Every game state mutation
flows through `entt::registry`. The entire working set fits in L1 cache.*

*Next step: implement `scroll_system` and `collision_system` first — they are
the core loop. Add rendering. Add input. Then layer scoring and burnout on top.*
