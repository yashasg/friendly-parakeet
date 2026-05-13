# SHAPESHIFTER — Technical Architecture Document
## Data-Oriented Design · C++20 · raylib · EnTT v3.16.0

> **Guiding principle**: Data lives in flat structs. Logic lives in free functions.
> The `entt::registry` is the single source of truth. No globals. No virtuals.

> **Current scoring model:** SHAPESHIFTER uses **on-beat timing x chain**.
> Timing grade (Perfect/Good/Ok/Bad) is computed from the input's distance to
> the beat when an obstacle resolves. Shape changes that land on the beat are
> valid play even when no obstacle is currently arriving; there is no penalty
> for "early" shape changes. See `rhythm-design.md` and `rhythm-spec.md` for
> the authoritative rhythm/scoring model, and `energy-bar.md` for survival HUD.
>
> **Historical note:** the removed Burnout risk/reward model is archived
> history only. It is not active runtime architecture.

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
    │   x=60     x=360    x=660   │
    │                             │
    │          obstacles          │
    │          scroll ↓           │
    │                             │
    │          ● player           │
    │          y=920              │
    │─────────────────────────────│
    │   energy bar     y=790      │
    │─────────────────────────────│
    │  [ ● ]   [ ■ ]   [ ▲ ]     │
    │          y=1140             │
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
    constexpr float LANE_X[3]         = { 60.0f, 360.0f, 660.0f };
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

    // ── Scoring ───────────────────────────────────────
    constexpr int   PTS_SHAPE_GATE    = 200;
    constexpr int   PTS_LANE_BLOCK    = 100;
    constexpr int   PTS_COMBO_GATE    = 200;
    constexpr int   PTS_SPLIT_PATH    = 300;
    constexpr float CHAIN_MULT_STEP    = 0.05f;
    constexpr int32_t CHAIN_MULT_BONUS_STEPS_CAP = 20; // caps at 2.0x

    // ── Energy Bar ────────────────────────────────────
    constexpr float ENERGY_MAX             = 1.0f;
    constexpr float ENERGY_START           = 1.0f;
    constexpr float ENERGY_DRAIN_MISS      = 0.20f;
    constexpr float ENERGY_DRAIN_BAD       = 0.05f;
    constexpr float ENERGY_RECOVER_OK      = 0.02f;
    constexpr float ENERGY_RECOVER_GOOD    = 0.05f;
    constexpr float ENERGY_RECOVER_PERFECT = 0.10f;

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

/// Authoritative world-space spatial state for moving/rendered entities.
/// Iterated by: scroll_system, render_system, collision_system,
///              obstacle_despawn_system
struct WorldTransform {
    glm::vec2 position = {0.0f, 0.0f};
    float     rotation = 0.0f;
    glm::vec2 scale    = {1.0f, 1.0f};
};

/// Movement vector in pixels/second for entities that integrate position.
/// Iterated with WorldTransform by motion_system every frame.
/// Rhythm obstacles use BeatInfo and are positioned by scroll_system instead.
struct MotionVelocity {
    glm::vec2 value = {0.0f, 0.0f};
};
```

### 2.2 — HOT: Player State (read every frame for collision + render)

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
/// Hot: read by shape_window_system, collision_system, render_system.
struct PlayerShape {
    Shape    current;      // active gameplay shape
    Shape    previous;     // for morph animation
    float    morph_t;      // 0.0 = previous, 1.0 = current (animation lerp)
};

/// Lane occupancy and transition. 8 bytes.
/// Hot: read by collision_system, player input handlers, render_system.
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

/// What action this obstacle demands and its base score.
/// Read by collision_system, miss_detection_system, scoring_system.
/// LowBar/HighBar were removed from the runtime obstacle enum and remain archival only.
enum class ObstacleKind : uint8_t {
    ShapeGate,   // must match shape
    LaneBlock,   // legacy value kept for backward compat
    ComboGate,   // shape + lane
    SplitPath,   // shape + specific lane
    OnsetMarker, // visual beat marker
};

struct Obstacle {
    int16_t      base_points = 200;   // PTS_SHAPE_GATE etc.
};
// Runtime kind is derived from optional components (RequiredShape,
// BlockedLanes, RequiredLane) via obstacle_kind_from_components().
// Existential tag: presence means the obstacle has been cleared and awaits scoring.
struct ScoredTag {};

// Existential tag: obstacle does not participate in score popups or chains.
// OnsetMarker obstacles carry this tag so beat markers remain visual-only.
struct NonScorableTag {};
```

### 2.4 — WARM: Obstacle Specifics (read by collision/scoring, not by scroll)

```cpp
// components/obstacle_data.h

/// For ShapeGate / ComboGate / SplitPath: which shape is required. 1 byte.
struct RequiredShape {
    Shape shape;
};

/// The push direction is implicit in the ObstacleKind, so this struct
/// is no longer needed. (Legacy BlockedLanes removed with LaneBlock.)

/// For SplitPath: which lane has the opening. 1 byte.
struct RequiredLane {
    int8_t lane;    // 0, 1, or 2
};

/// No vertical-bar action component exists in the current runtime.
/// LowBar/HighBar authoring references are archival/future design notes only.
```

### 2.5 — HOT: Fixed-lifetime FX timers

```cpp
// components/particle.h

/// Particle-specific render and expiry data.
struct ParticleData {
    float size      = 4.0f;
    float remaining = 0.0f;
    float max_time  = 0.0f;
};
```

### 2.6 — COLD: Rhythm, Scoring & Energy (singletons via registry.ctx())

```cpp
// components/scoring.h

/// Singleton: overall score state. Written by scoring_system only.
struct ScoreState {
    int32_t  score;              // banked score
    int32_t  displayed_score;    // for smooth scroll-up animation
    int32_t  high_score;         // persisted across sessions
    int32_t  chain_count;        // consecutive obstacles cleared
    float    chain_timer;        // seconds since last clear, retained across rests
    float    distance_traveled;  // total pixels scrolled (for distance bonus)
};

/// Score popup entity component. 8 bytes.
struct ScorePopup {
    int32_t    value           = 0;              // points to display
    bool       has_timing_tier = false;          // false when no TimingGrade was present
    TimingTier timing_tier     = TimingTier::Ok; // valid only when has_timing_tier is true
    float      remaining       = 0.0f;           // seconds left to display
    float      max_time        = 0.0f;           // initial lifetime for fade/scale
};
```

```cpp
// components/song_state.h

/// Singleton: runtime song timing and beat scheduling cursor.
struct SongState {
    float bpm             = 120.0f;
    float offset          = 0.0f;
    int   lead_beats      = 4;
    float beat_period     = 0.5f;
    float lead_time       = 2.0f;
    float scroll_speed    = constants::APPROACH_DIST / lead_time;
    float window_duration = 0.3f;
    float half_window     = 0.15f;
    float morph_duration  = 0.1f;
    float song_time       = 0.0f;
    int   current_beat    = -1;
    bool  playing         = false;
    bool  finished        = false;
    size_t next_spawn_idx = 0;
};

/// Singleton: survival meter displayed by the HUD energy bar.
struct EnergyState {
    float energy      = constants::ENERGY_START;
    float display     = constants::ENERGY_START;
    float flash_timer = 0.0f;
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

/// Semantic input events — produced by input/UI/test-player systems and
/// drained by game_state_system through EnTT dispatcher listeners.
struct GoEvent {
    Direction dir = Direction::Up;
};

struct ButtonPressEvent {
    ButtonPressKind kind = ButtonPressKind::Shape;
    Shape shape = Shape::Circle;  // valid when kind == Shape
    MenuActionKind menu_action = MenuActionKind::Confirm;  // valid when kind == Menu
    uint8_t menu_index = 0;  // valid when kind == Menu
};
```

### 2.8 — COLD: Game State (singleton)

```cpp
// components/game_state.h

enum class GamePhase : uint8_t {
    Title        = 0,
    LevelSelect  = 1,
    Playing      = 2,
    Paused       = 3,
    GameOver     = 4,
    SongComplete = 5,
    Settings     = 6,
    Tutorial     = 7
};

enum class EndScreenChoice : uint8_t {
    None = 0,
    Restart = 1,
    LevelSelect = 2,
    MainMenu = 3
};

/// Singleton: governs which systems run and screen transitions.
struct GameState {
    GamePhase  phase;
    GamePhase  previous_phase;
    float      phase_timer;          // seconds in current phase
    bool       transition_pending;   // set to request a phase change
    GamePhase  next_phase;           // destination of pending transition
    float      transition_alpha;     // 0..1 for fade effects
    EndScreenChoice end_choice;      // pending end-screen menu action
};
```

### 2.9 — Legacy: Difficulty (removed)

```cpp
// DifficultyConfig / difficulty_system were removed from the current song-mode
// runtime. Authored beatmaps and SongState now provide timing and spawn cadence.
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

/// Particle-specific data (WorldTransform, MotionVelocity, Color are separate).
struct ParticleData {
    float size;            // base rendered size
    float remaining;       // seconds until destroy
    float max_time;        // original duration, for alpha/size fades
};

/// Empty tag — marks particle entities. 0 bytes.
struct ParticleTag {};
```

Score feedback particles are spawned by `scoring_system` when a scored obstacle
is processed. `particle_system` owns lifetime/gravity and `camera_system` builds
their effects-pass `ModelTransform` for rendering.

### 2.12 — COLD: Audio (singleton)

```cpp
// audio/audio_types.h

/// Sound effect identifiers — map to loaded Sound handles in audio init.
enum class SFX : uint8_t {
    ShapeShift = 0,
    Crash,
    UITap,
    ChainBonus,
    ScorePopup,
    GameStart,
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
WorldTransform      20     HOT        motion, render, collision, despawn
MotionVelocity       8     HOT        motion, particle effects
ParticleData        12     HOT        particle expiry/render fade
ScorePopup          16     HOT        popup expiry/render fade
PlayerTag            0     HOT        collision/filter
PlayerShape          4     HOT        shape_window, collision, render, player_action
Lane                 8     HOT        collision, render, player_action
VerticalState       12     HOT        collision, render, player_action
ObstacleTag          0     HOT        scroll, collision, cleanup
Obstacle             4     WARM       collision, miss_detection, scoring
RequiredShape        1     WARM       collision, scoring
BlockedLanes         1     WARM       collision
RequiredLane         1     WARM       collision
Color                4     COLD       render
DrawSize             8     COLD       render
DrawLayer            1     COLD       render
ScorePopup           5     COLD       render, scoring
ParticleData         8     COLD       render
ParticleTag          0     COLD       render (filter)
─────────────────────────────────────────────────────────────
SINGLETONS (ctx)
─────────────────────────────────────────────────────────────
InputState          36     per-frame  input_system internal touch/mouse state
entt::dispatcher    var    per-frame  input/UI/test-player enqueue semantic events, game_state drains
ButtonPress/Go      var    per-frame  dispatcher payloads handled by game_state/player input listeners
GameState           12     per-frame  game_state_system
BeatMap             var    session    setup_play_session (write), beat_scheduler (read)
SongState           48     per-frame  song_playback/beat_scheduler
EnergyState         12     per-frame  energy_system (write), ui_render (read)
ScoreState          28     per-frame  scoring_system (write), render (read)
PlaySfxEvent        var    per-frame  dispatcher events drained by audio_system
─────────────────────────────────────────────────────────────
```

---

## 3. System Execution Order

Every scheduled system is a free function: `void name(entt::registry& reg, float dt)`.
Input reactions that mutate player components are dispatcher callbacks wired by
`wire_input_dispatcher`, not a separately scheduled `player_input_system`.
Systems run in strict order. No scheduled system reads data written by a later
system in the same frame (unidirectional data flow).

```
 Frame N
 ═══════════════════════════════════════════════════════════════
 │
 │  ┌─ PHASE 1: INPUT CAPTURE ──────────────────────────────┐
 │  │                                                        │
 │  │  1. compute_screen_       Refresh logical-to-window    │
 │  │     transform             scale and letterbox offsets. │
 │  │                                                        │
 │  │  2. input_system          Read raylib input queue.     │
 │  │                           Update InputState and enqueue│
 │  │                           GoEvent/ButtonPressEvent.    │
 │  │                                                        │
 │  │  3. other producers       gameplay_hud_process_button_ │
 │  │                           input and test_player_system │
 │  │                           enqueue the same semantic    │
 │  │                           events.                      │
 │  └────────────────────────────────────────────────────────┘
 │
 │  ┌─ PHASE 2: GAME STATE GATE ────────────────────────────┐
 │  │                                                        │
 │  │  4. game_state_system     Drain Go/ButtonPress events;│
 │  │                           listeners update player and  │
 │  │                           menu state in connection     │
 │  │                           order. Process transitions.  │
 │  │                           On entry to PLAYING: spawn    │
 │  │                           player, reset singletons.    │
 │  │                           On PLAYING→GAME_OVER: save   │
 │  │                           high score, spawn crash fx.  │
 │  │                           Updates phase_timer.         │
 │  │                                                        │
 │  │  ── if phase != Playing, tick_playing_systems skips ── │
 │  └────────────────────────────────────────────────────────┘
 │
 │  ┌─ PHASE 3: PLAYBACK + PLAYING TICK ────────────────────┐
 │  │                                                        │
 │  │  5. song_playback_system  Advance SongState.song_time  │
 │  │                           and current_beat from music. │
 │  │                                                        │
 │  │  6. tick_playing_systems  Runs only in Playing phase:  │
 │  │                                                        │
 │  │     a. beat_log_system    Record session beat telemetry.│
 │  │                                                        │
 │  │     b. beat_scheduler_sys Spawn authored BeatMap notes │
 │  │                           whose spawn_time has arrived.│
 │  │                                                        │
 │  │     c. shape_window_activation                          │
 │  │                           Promote completed MorphIn     │
 │  │                           windows before collision.     │
 │  │                                                        │
 │  │     d. player_movement    Advance lane and jump/slide. │
 │  │                                                        │
 │  │     e. scroll_system      Derive rhythm-obstacle       │
 │  │                           positions from SongState     │
 │  │                           song_time + BeatInfo.        │
 │  │                                                        │
 │  │     f. motion_system      For every (WorldTransform,   │
 │  │                           MotionVelocity):             │
 │  │                           position += velocity * dt.   │
 │  │                           Simple, tight inner loop.    │
 │  │                                                        │
 │  │     g. collision_system   For each obstacle near       │
 │  │                           PLAYER_Y: test shape match,  │
 │  │                           lane match, vertical state.  │
 │  │                           On match: emplace TimingGrade│
 │  │                           and ScoredTag.               │
 │  │                                                        │
 │  │     h. shape_window_sys   Advance active timing windows│
 │  │                           and morph interpolation.     │
 │  │                                                        │
 │  │     i. miss_detection     Mark passed unresolved notes │
 │  │                           with MissTag/ScoredTag.      │
 │  │                                                        │
 │  │     j. scoring_system     Process scored obstacles.    │
 │  │                           Apply timing multiplier and  │
 │  │                           chain bonus. Queue popup     │
 │  │                           requests. Update SongResults.│
 │  │                           Positive popups emit SFX.    │
 │  └────────────────────────────────────────────────────────┘
 │
 │  ┌─ PHASE 4: CLEANUP & FX ──────────────────────────────┐
 │  │                                                        │
 │  │  7. obstacle_despawn_     Destroy obstacles past the   │
 │  │     system                camera Z / DESTROY_Y limit.  │
 │  │                                                        │
 │  │  8. popup_feedback_system Spawn score/feedback popups  │
 │  │                           from queued requests.        │
 │  │                                                        │
 │  │  9. popup_display_system  Tick ScorePopup.remaining.   │
 │  │                           Fade and destroy popups.     │
 │  │                                                        │
 │  │ 10. energy_system         Apply pending energy changes │
 │  │                           and smooth displayed energy. │
 │  │                                                        │
 │  │ 11. energy_bar_system     Prepare energy bar visual    │
 │  │                           animation state.             │
 │  │                                                        │
 │  │ 12. particle_system       Tick ParticleData.remaining. │
 │  │                           Destroy expired particles.   │
 │  │                           Apply gravity to survivors.  │
 │  └────────────────────────────────────────────────────────┘
 │
 │  ┌─ PHASE 5: RENDER (always runs) ──────────────────────┐
 │  │                                                        │
 │  │ 13. game_camera_system    Update gameplay camera after │
 │  │     ui_camera_system      fixed-step transforms settle.│
 │  │                                                        │
 │  │ 14. render systems        Draw world and UI render     │
 │  │                           targets, then composite both │
 │  │                           to the window.               │
 │  │                                                        │
 │  │ 15. audio_system          Drain PlaySfxEvent dispatcher│
 │  │                           events after rendering.      │
 │  │                                                        │
 │  │ 16. haptic_system         Play queued haptic feedback. │
 │  └────────────────────────────────────────────────────────┘
 │
 ═══════════════════════════════════════════════════════════════
```

### System → Component Access Notes

`app/systems/all_systems.h` is the authoritative declaration list for runtime
systems. Fixed-lifetime effects no longer use a shared generic timer component:
`particle_system` owns `ParticleData::remaining`, and `popup_display_system`
owns `ScorePopup::remaining`. Obstacle destruction is handled by
`obstacle_despawn_system`, which reads `WorldTransform` for obstacle position.

---

## 4. Game State Machine

```
            ┌────────────────────────────────────────────────────┐
            │                                                    │
            ▼                                                    │
     ╔═══════════╗   start    ╔══════════════╗   confirm   ╔════════════╗
     ║   TITLE   ║ ─────────▶ ║ LEVEL_SELECT ║ ──────────▶ ║  PLAYING   ║
     ╚═══════════╝            ╚══════════════╝             ╚════════════╝
            ▲                        ▲                       │    │    │
            │                        │             pause/app │    │    │ song
            │                        │                       ▼    │    ▼ done
            │                        │                  ╔══════════╗ ╔══════════════╗
            │                        └───────────────── ║  PAUSED  ║ ║ SONG_COMPLETE║
            │                                           ╚══════════╝ ╚══════════════╝
            │                                                │              │
            │                       energy depleted          │              │
            │                                                ▼              │
            │                                           ╔══════════╗        │
            └────────────────────────────────────────── ║ GAMEOVER ║ ◀──────┘
                                                        ╚══════════╝
     TITLE also routes to SETTINGS and TUTORIAL; end screens can restart,
     return to LEVEL_SELECT, or return to TITLE.
```

### Transition: menu flow → PLAYING

```cpp
void enter_playing(entt::registry& reg) {
    // 1. Destroy any lingering entities and spawn canonical cameras/player.
    setup_play_session(reg); // clears registry, loads BeatMap, resets ScoreState,
                             // SongState, EnergyState, SongResults, and player.

    // 2. Update game state. The state system owns this phase transition.
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

    // 3. Do NOT destroy obstacles — they freeze in place for dramatic effect
    //    scroll_system will skip because phase != Playing

    // 4. Transition
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
In code, reusable construction lives in `app/entities/` factory functions
(`create_player_entity`, `spawn_obstacle`, etc.).

### 5.1 Player Entity (always exactly 1)

```
┌─ Player ──────────────────────────────────────────────────┐
│ PlayerTag          (tag, 0 bytes)                         │
│ WorldTransform     { position: {360.0, 920.0} }          │
│ PlayerShape        { current: Circle, prev: Circle,       │
│                      morph_t: 1.0 }                       │
│ Lane               { current: 1, target: -1, lerp_t: 1 } │
│ VerticalState      { mode: Grounded, timer: 0, y_off: 0 }│
│ Color              { r: 80, g: 180, b: 255, a: 255 }     │
│ DrawSize           { w: 64, h: 64 }                       │
│ DrawLayer          { layer: Game }                         │
└───────────────────────────────────────────────────────────┘
Total: ~73 bytes per entity (1 entity)
```

### 5.2 Shape Gate Entity

```
┌─ Shape Gate ──────────────────────────────────────────────┐
│ ObstacleTag        (tag, 0 bytes)                         │
│ WorldTransform     { position: {360.0, -120.0} }         │
│ MotionVelocity     { value: {0.0, 400.0} }               │
│ Obstacle           { kind: ShapeGate, base_pts: 200,      │
│                      scored: false }                       │
│ RequiredShape      { shape: Triangle }                     │
│ Color              { r: 50, g: 205, b: 50, a: 255 }      │
│ DrawSize           { w: 720, h: 80 }                       │
│ DrawLayer          { layer: Game }                          │
└───────────────────────────────────────────────────────────┘
Total: ~42 bytes per entity (5–15 active)
```


```
│ ObstacleTag        (tag, 0 bytes)                         │
│ WorldTransform     { position: {360.0, -120.0} }         │
│ MotionVelocity     { value: {0.0, 400.0} }               │
│                      scored: false }                       │
│ Color              { r: 255, g: 60, b: 60, a: 255 }      │
│ DrawSize           { w: 720, h: 80 }                       │
│ DrawLayer          { layer: Game }                          │
└───────────────────────────────────────────────────────────┘
Total: ~40 bytes per entity

obstacle's direction on beat arrival. No player action required.
- Only affects player if they are on the SAME lane as the obstacle.
- Awards 0 points (it's not a challenge).
```

### 5.4 Archived Vertical Bar Entity (Low Bar / High Bar)

LowBar/HighBar entity archetypes are historical only. The current runtime enum, components, and beatmap editor do not expose them as authorable obstacle kinds; use archived design docs if this future design space is reconsidered.

### 5.5 Combo Gate Entity (Shape + Lane)

```
┌─ Combo Gate ──────────────────────────────────────────────┐
│ ObstacleTag        (tag, 0 bytes)                         │
│ WorldTransform     { position: {360.0, -120.0} }         │
│ MotionVelocity     { value: {0.0, 400.0} }               │
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
│ WorldTransform     { position: {360.0, -120.0} }         │
│ MotionVelocity     { value: {0.0, 400.0} }               │
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
│ WorldTransform     { position: {360.0, 900.0} }          │
│ MotionVelocity     { value: {0.0, -80.0} } (floats up)   │
│ ScorePopup         { value: 600, tier: 3, remaining: 1.2 } │
│ Color              { r: 255, g: 200, b: 50, a: 255 }     │
│ DrawLayer          { layer: Effects }                      │
└───────────────────────────────────────────────────────────┘
Total: ~33 bytes per entity (0–5 active)
```

### 5.8 Particle Entity

```
┌─ Particle ────────────────────────────────────────────────┐
│ ParticleTag        (tag, 0 bytes)                         │
│ WorldTransform     { position: {360.0, 920.0} }          │
│ MotionVelocity     { value: {rand, rand} } (random burst) │
│ ParticleData       { size: 4.0, remaining: 0.6, max_time: 0.6 } │
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
    reg.ctx().emplace<entt::dispatcher>();
    wire_input_dispatcher(reg);
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

        compute_screen_transform(reg);

        // ── INPUT (once per frame, outside fixed loop) ──
        input_system(reg, raw_dt);
        gameplay_hud_process_button_input(reg);
        test_player_system(reg, raw_dt);

        // ── FIXED TIMESTEP LOOP ──────────────────────────────
        while (accumulator >= FIXED_DT) {
            game_state_system(reg, FIXED_DT);
            song_playback_system(reg, FIXED_DT);
            tick_playing_systems(reg, FIXED_DT);
            obstacle_despawn_system(reg, FIXED_DT);
            popup_feedback_system(reg, FIXED_DT);
            popup_display_system(reg, FIXED_DT);
            energy_system(reg, FIXED_DT);
            energy_bar_system(reg, FIXED_DT);
            particle_system(reg, FIXED_DT);

            accumulator -= FIXED_DT;
        }

        // ── RENDER (once per frame, variable rate) ────────────
        game_camera_system(reg, raw_dt);
        ui_camera_system(reg, raw_dt);
        game_render_system(reg, 0.0f);
        ui_render_system(reg, 0.0f);

        // ── AUDIO (once per frame, after render) ──────────────
        audio_system(reg);
        haptic_system(reg);
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
│ HUD button processing          │ Variable │ UI events│
│ test_player_system              │ Variable │ Automation│
│ game_state_system              │ Fixed    │ Logic    │
│ song_playback_system           │ Fixed    │ Timing   │
│ beat_log_system                │ Fixed    │ Telemetry│
│ beat_scheduler_system          │ Fixed    │ Logic    │
│ shape_window_activation_system │ Fixed    │ Timing   │
│ player_movement_system         │ Fixed    │ Physics  │
│ scroll_system                  │ Fixed    │ Physics  │
│ motion_system                  │ Fixed    │ Physics  │
│ collision_system               │ Fixed    │ Physics  │
│ shape_window_system            │ Fixed    │ Timing   │
│ miss_detection_system          │ Fixed    │ Logic    │
│ scoring_system                 │ Fixed    │ Logic    │
│ obstacle_despawn_system        │ Fixed    │ Cleanup  │
│ popup_feedback_system          │ Fixed    │ FX       │
│ popup_display_system           │ Fixed    │ FX       │
│ energy_system                  │ Fixed    │ Logic    │
│ energy_bar_system              │ Fixed    │ UI state │
│ particle_system                │ Fixed    │ FX       │
│ render systems                 │ Variable │ Display  │
│ audio_system                   │ Variable │ Playback │
│ haptic_system                  │ Variable │ Feedback │
└────────────────────────────────┴──────────┴──────────┘
```

---

## 7. Data Flow Diagrams

### 7.1 Critical Path: BeatMap → Timing Grade → Score/Energy

```
    BEATMAP/SINGLET                 OBSTACLE ENTITY              PLAYER ENTITY
    ┌──────────────┐                                             ┌─────────────┐
    │ BeatMap      │                                             │ PlayerShape │
    │ SongState    │                                             │ Lane        │
    └──────┬───────┘                                             │ VertState   │
           │ beat_scheduler_system                               │ WorldTransform │
           │ creates note when spawn_time arrives                └──────┬──────┘
           ▼                                                            │
    ┌──────────────┐                                                    │
    │ ObstacleTag  │                                                    │
    │ Obstacle     │                                                    │
    │ RequiredShape│                                                    │
    │ BeatInfo     │ arrival_time from authored beat                    │
    └──────┬───────┘                                                    │
           │ scroll/motion systems                                      │
           ▼                                                            │
    ┌──────────────┐                                                    │
    │ obstacle     │  collision_system checks player state when note    │
    │ approaches   │  reaches the active window. On clear:              │
    └──────┬───────┘                                                    │
           │                                                            │
           ▼                                                            │
    ┌────────────────────────────────────────────────────────────────────┐
    │ TimingGrade { Perfect | Good | Ok | Bad } + ScoredTag             │
    │ precision = distance from input/song time to BeatInfo.arrival_time │
    └──────────┬─────────────────────────────────────────────────────────┘
               │
               ▼
    ┌────────────────────────────────────────────────────────────────────┐
    │ scoring_system                                                     │
    │ - points = base_points x timing_multiplier x chain_multiplier      │
    │ - updates ScoreState and SongResults                               │
    │ - queues ScorePopupRequest for popup_feedback_system               │
    └──────────┬─────────────────────────────────────────────────────────┘
               │
               ▼
    ┌────────────────────────────────────────────────────────────────────┐
    │ energy_system                                                      │
    │ - MISS/BAD drain EnergyState                                       │
    │ - OK/GOOD/PERFECT recover EnergyState                              │
    │ - energy <= 0 requests GameOver                                    │
    └────────────────────────────────────────────────────────────────────┘
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
           │           │ GAMEPLAY HUD CONTROL ROUTING         │
           ├──────────▶│ raygui/controller resolves tap to    │
           │           │ shape control ID (e.g. Square) and   │
           │           │ emits semantic button activation.     │
           │           └──────────────────┬───────────────────┘
           │                              │
           │                              ▼
           │           ┌──────────────────────────────────────┐
           │           │ entt::dispatcher                     │
           │           │   ButtonPressEvent{Shape, Square}    │
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
    │ entt::dispatcher        │          │
    │   GoEvent{Left}         │          │
    └──────────┬───────────────┘          │
               │                          │
               ▼                          ▼
    ┌────────────────────────────────────────────────────────┐
    │ dispatcher listeners:                                  │
    │                                                        │
    │   // 1. Process shape button                           │
    │   player_input_handle_press(ButtonPressEvent) {        │
    │       PlayerShape.previous = PlayerShape.current;      │──▶ PlayerShape
    │       PlayerShape.current  = event.shape;              │    { current: Square,
    │       PlayerShape.morph_t  = 1.0f;                     │      previous: Circle,
    │       disp.enqueue(PlaySfxEvent{ShapeShift});          │      morph_t: 1.0 }
    │   }                                                    │
    │                                                        │
    │   // 2. Process direction                              │
    │   player_input_handle_go(GoEvent) {                    │
    │       if (event.dir == Left) {                         │
    │           if (Lane.current > 0) {                      │──▶ Lane
    │               Lane.target = Lane.current - 1;          │    { current: 1,
    │               Lane.lerp_t = 0.0f;                      │      target: 0,
    │           }                                            │      lerp_t: 0.0 }
    │       } else if (event.dir == Up) {                    │
    │           if (VertState.mode == Grounded) {             │──▶ VerticalState
    │               VertState.mode  = Jumping;               │    { mode: Jumping,
    │               VertState.timer = JUMP_DURATION;         │      timer: 0.45,
    │           }                                            │      y_offset: 0.0 }
    │       }                                                │
    │       // ... Right, Down ...                           │
    │   }                                                    │
    └────────────────────────────────────────────────────────┘
```

### 7.3 Proximity Ring and Energy Feedback (detail)

```
    ui_render_system(reg):

    SongState.song_time  ─┐
    BeatMap.beats        ├─ find next unresolved note near the active shape
    PlayerShape.current ─┘

    proximity = clamp((arrival_time - song_time) / SongState.half_window)
    draw ring around the corresponding shape button:
        wide/faint   = outside timing window
        tightening   = approaching beat
        centered     = on-beat hit opportunity

    energy_system(reg, dt):

    ScoreState/SongResults events from scoring and miss_detection
        BAD or MISS       -> drain EnergyState.energy
        OK/GOOD/PERFECT   -> recover EnergyState.energy

    EnergyState.display lerps toward EnergyState.energy for the HUD bar.
    EnergyState.flash_timer marks drain feedback. If energy <= 0,
    energy_system requests the terminal GameOver phase.
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
  │  ■ WorldTransform  (motion, render, collision, every frame, R+W)
  │  ■ MotionVelocity  (motion + particle, every frame, R+W)
  │  ■ ParticleData    (particle sys, every frame, R+W)
  │  ■ ScorePopup      (popup sys, every frame, R+W)
  │
  │  □ PlayerShape     (shape window + collision + render, every frame, R)
  │  □ Lane            (collision + render, every frame, R)
  │  □ VerticalState   (collision + render, every frame, R)
  │  □ Obstacle        (collision + scoring, every frame, R)
  │
  │  ○ RequiredShape   (collision + scoring, per-obstacle, R)
  │  ○ BlockedLanes    (collision, per-obstacle, R)
  │  ○ Color           (render only, R)
  │  ○ DrawSize        (render only, R)
  │
  │  · ParticleData    (particle sys only, R+W)
  │  · ScorePopup      (render only, R)
  └──────────────────────────────────────────────▶  ENTITY COUNT
     1 (player)          15 (obstacles)     50 (particles)
```

### 8.3 EnTT Storage Model: Why AoS is Fine Here

EnTT stores each component type in its own **sparse set** — effectively a
`std::vector<T>` per component type with an indirection layer. This gives us:

```
WorldTransform pool: [xf0] [xf1] [xf2] [xf3] ... [xfN]    contiguous in memory
MotionVelocity pool: [vel0] [vel1] [vel2] ... [velN]      contiguous in memory
Obstacle pool:   [obs0] [obs1] [obs2] ... [obsM]          contiguous in memory
```

**For SHAPESHIFTER, AoS (EnTT default) is optimal because:**

1. **Entity counts are tiny** (~71 peak). The whole `WorldTransform` pool is
   roughly 20 bytes × 71 = 1.4 KB. There is no cache pressure that SoA would
   alleviate.

2. **Iteration patterns are simple**. The hottest loop (`motion_system`) reads
   `WorldTransform` + `MotionVelocity` together — two tiny pools that are in L1
   after the first iteration.

3. **SoA (splitting `WorldTransform::position` into x[] and y[])** would add
   indirection overhead and code complexity for zero performance gain at these
   entity counts. SoA becomes beneficial at 10,000+ entities — we peak at 71.

The current implementation uses an EnTT view for the small dt-integrated set:

```cpp
void motion_system(entt::registry& reg, float dt) {
    auto motion_view = reg.view<WorldTransform, MotionVelocity>();
    for (auto [entity, transform, velocity] : motion_view.each()) {
        transform.position.x += velocity.value.x * dt;
        transform.position.y += velocity.value.y * dt;
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
│ beat_scheduler        │ < 0.05 ms     │ 0-N song obstacles/tick    │
│ scroll_system         │ < 0.05 ms     │ ~71 entities, 2 floats    │
│ collision_system      │ < 0.10 ms     │ ~15 obstacles × player    │
│ miss_detection        │ < 0.05 ms     │ ~15 obstacles              │
│ scoring_system        │ < 0.05 ms     │ 0-1 scores/frame          │
│ particle_system       │ < 0.10 ms     │ ~50 particles             │
│ obstacle_despawn      │ < 0.05 ms     │ scan + destroy            │
│ popup_display         │ < 0.05 ms     │ ~5 score popups           │
│ render systems        │ < 1.50 ms     │ raylib draw calls (GPU)   │
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
    auto player_view = reg.view<PlayerTag, WorldTransform, PlayerShape, Lane, VerticalState>();
    // Early-out: exactly one player
    auto [p_ent, p_pos, p_shape, p_lane, p_vstate] = *player_view.each().begin();

    float player_x = p_pos.position.x;
    float player_y = p_pos.position.y + p_vstate.y_offset;

    // Linear scan of obstacles — 15 entities max
    auto obs_view = reg.view<ObstacleTag, WorldTransform, Obstacle>();
    for (auto [entity, o_pos, obs] : obs_view.each()) {
        if (obs.scored) continue;

        // Broad phase: vertical proximity check
        float dy = o_pos.position.y - player_y;
        if (dy < -40.0f || dy > 40.0f) continue;  // not overlapping vertically

        // Narrow phase: per-obstacle-kind match test
        bool cleared = check_obstacle_cleared(reg, entity, obs,
                                               p_shape, p_lane);
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
                             const Lane& lane) {
    switch (obs.kind) {
        case ObstacleKind::ShapeGate:
            return shape.current == reg.get<RequiredShape>(e).shape;

        case ObstacleKind::LaneBlock:
            return true;   // passive — push is applied automatically

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
            auto view = reg.view<ObstacleTag, WorldTransform, Obstacle, Color, DrawSize>();
            for (auto [e, pos, obs, col, sz] : view.each()) {
                draw_obstacle(pos, obs, col, sz, reg, e);
            }
        }

        // Player (1 entity)
        {
            auto view = reg.view<PlayerTag, WorldTransform, PlayerShape,
                                  Lane, VerticalState, Color, DrawSize>();
            for (auto [e, pos, shape, lane, vs, col, sz] : view.each()) {
                // Interpolate position for smooth sub-frame rendering
                float render_x = pos.position.x;  // lane lerp already applied
                float render_y = pos.position.y + vs.y_offset;
                draw_player_shape(render_x, render_y,
                                  shape, col, sz);
            }
        }
    }

    // ── Layer 2: Effects ──────────────────────────────
    // Particles
    {
        auto view = reg.view<ParticleTag, WorldTransform, ParticleData, Color>();
        for (auto [e, transform, pd, col] : view.each()) {
            float t = pd.remaining / pd.max_time;
            uint8_t a = static_cast<uint8_t>(col.a * t);
            float size = pd.size * t;
            draw_particle(transform.position.x, transform.position.y, size,
                          {col.r, col.g, col.b, a});
        }
    }

    // Score popups
    {
        auto view = reg.view<ScorePopup, WorldTransform>();
        for (auto [e, popup, pos] : view.each()) {
            float t = popup.remaining / popup.max_time;
            draw_score_popup(pos.position.x, pos.position.y, popup.value, popup.tier, t);
        }
    }

    // ── Layer 3: HUD ──────────────────────────────────
    if (phase == GamePhase::Playing || phase == GamePhase::Paused) {
        auto& score  = reg.ctx().get<ScoreState>();
        auto& energy = reg.ctx().get<EnergyState>();
        draw_hud_score(score);
        draw_energy_bar(energy);
        draw_proximity_ring(reg);
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
│   ├── transform.h              ← WorldTransform, MotionVelocity, UIPosition
│   ├── player.h                 ← PlayerTag, PlayerShape, Lane, VerticalState
│   ├── obstacle.h               ← obstacle tags/data and requirements
│   ├── scoring.h                ← ScoreState, ScorePopup
│   ├── input.h                  ← InputState, Direction
│   ├── input_events.h           ← ButtonPressEvent, GoEvent, UI button data
│   ├── game_state.h             ← GameState, GamePhase, LevelSelectState
│   ├── rendering.h              ← DrawSize, DrawLayer, screen/model transforms
│   ├── particle.h               ← ParticleData, ParticleTag
│   ├── rhythm.h                 ← BeatInfo, TimingGrade, TimingTier
│   └── song_state.h             ← SongState
│
├── systems/                     ← all system free functions
│   ├── all_systems.h            ← convenience #include for all systems
│   ├── input_system.cpp         ← raylib polling → semantic dispatcher events
│   ├── game_state_system.cpp    ← phase transitions
│   ├── song_playback_system.cpp ← music stream timing
│   ├── beat_log_system.cpp      ← session beat telemetry
│   ├── player_input_system.cpp  ← ButtonPressEvent/GoEvent dispatcher callbacks
│   ├── test_player_system.cpp   ← automated test player (enqueues semantic events)
│   ├── player_movement_system.cpp ← lane lerp, jump parabola
│   ├── beat_scheduler_system.cpp ← song-authored obstacle entities
│   ├── scroll_system.cpp        ← derive rhythm positions from song_time + BeatInfo
│   ├── collision_system.cpp     ← obstacle vs player match test
│   ├── miss_detection_system.cpp ← missed obstacles → miss events
│   ├── scoring_system.cpp       ← bank points, chain, spawn popup
│   ├── particle_system.cpp      ← tick, gravity, cull particles
│   ├── obstacle_despawn_system.cpp ← off-camera obstacle removal
│   ├── popup_display_system.cpp ← tick, fade, cull popups
│   ├── game_render_system.cpp   ← world raylib draw calls
│   ├── ui_render_system.cpp     ← UI raylib draw calls
│   └── audio_system.cpp         ← AudioQueue → PlaySound
│
├── input/                       ← input routing/listeners and semantic gesture helpers
├── ui/                          ← UI layout loading, button spawning, controllers
├── audio/                       ← audio data and SFX bank helpers
├── session/                     ← play/test-player session setup
├── entities/                    ← canonical reusable entity factories/resources
├── rendering/                   ← render/camera resource context types
└── util/                        ← persistence, beatmap loading, session logging
```

---

## Appendix A: EnTT Patterns Used

### A.1 Singleton Access

```cpp
// Write (system that produces data):
auto& song = reg.ctx().get<SongState>();
song.song_time = calculated_value;

// Read (system that consumes data):
const auto& song = reg.ctx().get<const SongState>();
float t = song.song_time;
```

### A.2 View Iteration (most common)

```cpp
// Multi-component view — O(n) where n = smallest pool
auto view = reg.view<ObstacleTag, WorldTransform, Obstacle>();
for (auto [entity, pos, obs] : view.each()) {
    // ...
}
```

### A.3 View for Hot Path

```cpp
// Used in motion_system for dt-integrated entities.
auto motion_view = reg.view<WorldTransform, MotionVelocity>();
for (auto [entity, transform, velocity] : motion_view.each()) {
    transform.position.x += velocity.value.x * dt;
    transform.position.y += velocity.value.y * dt;
}
```

### A.4 Entity Creation (obstacle spawn)

```cpp
entt::entity spawn_rhythm_shape_gate(entt::registry& reg, Shape required,
                                     int8_t lane, const BeatInfo& beat_info) {
    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);
    reg.emplace<WorldTransform>(e, WorldTransform{{constants::LANE_X[lane], constants::SPAWN_Y}});
    reg.emplace<BeatInfo>(e, beat_info);
    reg.emplace<Obstacle>(e, int16_t{constants::PTS_SHAPE_GATE});
    reg.emplace<RequiredShape>(e, required);
    reg.emplace<Color>(e, shape_color(required));
    reg.emplace<DrawSize>(e, static_cast<float>(constants::SCREEN_W), 80.0f);
    reg.emplace<DrawLayer>(e, Layer::Game);
    return e;
}
```

### A.5 Entity Destruction

```cpp
// obstacle_despawn_system: destroy obstacles past the camera boundary
auto model_view = reg.view<ObstacleTag, ObstacleScrollZ>();
for (auto [entity, scroll] : model_view.each()) {
    if (scroll.z > camera_despawn_z) {
        reg.destroy(entity);   // safe during iteration in EnTT v3
    }
}

// Fixed-lifetime effects own their timers in domain components.
// particle_system ticks ParticleData::remaining.
// popup_display_system ticks ScorePopup::remaining.
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

constexpr Color TIMING_TIER_COLORS[4] = {
    { 255,  80,  80, 255 },  // Bad
    { 255, 220,  80, 255 },  // Ok
    {  80, 200, 255, 255 },  // Good
    { 255, 255, 255, 255 }   // Perfect
};
```

---

## Appendix C: Removed Burnout Model

The old Burnout risk/reward loop was removed by issue #239 and must not be
used for new runtime work.

Current replacements:

- **Scoring:** timing grade (Perfect/Good/Ok/Bad) x chain multiplier; see
  `rhythm-design.md` and `rhythm-spec.md`.
- **Survival meter:** `EnergyState` rendered as the energy bar; see
  `energy-bar.md`.
- **Live timing cue:** proximity ring around shape buttons; see
  `rhythm-spec.md` Section 6 and `game-flow.md`.

---

*This document defines the complete data layout. No component holds a pointer to
another component. No system calls a virtual method. Every game state mutation
flows through `entt::registry`. The entire working set fits in L1 cache.*

*Next step: implement `scroll_system` and `collision_system` first — they are
the core loop. Add rendering. Add input. Then layer rhythm scoring and energy on top.*
