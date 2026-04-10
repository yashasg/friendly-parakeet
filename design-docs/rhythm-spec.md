# SHAPESHIFTER — Rhythm-Core Technical Specification
## Implementation Details for Rhythm Engine · v1.0

> **Game design:** See `rhythm-design.md` for vision, player experience, and design rationale.
> **Depends on:** Burnout Scoring (SPEC 2), Input System (SPEC 1)

---

## Table of Contents

1. [Beat Map Format](#section-1--beat-map-format)
2. [BPM-Derived Constants](#section-2--bpm-derived-constants)
3. [ECS Components](#section-3--ecs-components)
4. [System Pipeline Changes](#section-4--system-pipeline-changes)
5. [Edge Cases](#section-5--edge-cases)
- [Appendix A — Implementation Phases](#appendix-a--implementation-phases)

---

# ═══════════════════════════════════════════════════
# SECTION 1 — BEAT MAP FORMAT
# ═══════════════════════════════════════════════════

## File Structure

```
  Beat maps are generated from audio using aubio (see rhythm-design.md §7),
  then hand-tuned by a designer. Each song has two files:

  assets/songs/
  ├── tutorial_groove/
  │   ├── song.ogg          ← the audio file
  │   └── beatmap.json      ← the beat chart
  ├── neon_highway/
  │   ├── song.ogg
  │   └── beatmap.json
  └── final_boss/
      ├── song.ogg
      └── beatmap.json
```

## Beat Map JSON Schema

```json
{
  "song_id":      "neon_highway",
  "title":        "Neon Highway",
  "artist":       "Shapeshifter OST",
  "bpm":          120,
  "offset":       0.200,
  "lead_beats":   4,
  "difficulty":   "medium",
  "duration_sec": 180.0,

  "beats": [
    { "beat": 4,  "kind": "shape_gate", "shape": "circle",   "lane": 1 },
    { "beat": 8,  "kind": "shape_gate", "shape": "triangle", "lane": 1 },
    { "beat": 12, "kind": "shape_gate", "shape": "square",   "lane": 1 },
    { "beat": 16, "kind": "shape_gate", "shape": "circle",   "lane": 0 },
    { "beat": 17, "kind": "lane_block", "blocked": [1, 2]              },
    { "beat": 20, "kind": "shape_gate", "shape": "triangle", "lane": 2 },
    { "beat": 24, "kind": "combo_gate", "shape": "circle",   "blocked": [0, 1] },
    { "beat": 28, "kind": "low_bar"                                    },
    { "beat": 32, "kind": "high_bar"                                   },
    { "beat": 36, "kind": "split_path", "shape": "square",   "lane": 2 }
  ]
}
```

## Field Reference

```
  ┌─────────────────┬────────────────────────────────────────────────────┐
  │  FIELD           │  DESCRIPTION                                      │
  ├─────────────────┼────────────────────────────────────────────────────┤
  │  song_id         │  Unique ID (also directory name in assets/songs/) │
  │  title           │  Display name for song select UI                  │
  │  artist          │  Credit line                                      │
  │  bpm             │  Beats per minute (fixed for the entire song)     │
  │  offset          │  Seconds from audio start to first beat           │
  │                  │  (accounts for intro/silence before downbeat)     │
  │  lead_beats      │  How many beats ahead obstacles spawn (default 4) │
  │  difficulty      │  "easy" / "medium" / "hard" / "insane"           │
  │  duration_sec    │  Total song length for progress bar               │
  ├─────────────────┼────────────────────────────────────────────────────┤
  │  beats[].beat    │  Beat number (0-indexed from first downbeat)      │
  │  beats[].kind    │  Obstacle type: shape_gate, lane_block,           │
  │                  │  low_bar, high_bar, combo_gate, split_path       │
  │  beats[].shape   │  "circle" / "square" / "triangle"                │
  │                  │  (required for shape_gate, combo_gate, split)    │
  │  beats[].lane    │  0/1/2 (left/center/right)                       │
  │                  │  (required for shape_gate, split_path)           │
  │  beats[].blocked │  Array of blocked lane indices [0,1,2]           │
  │                  │  (required for lane_block, combo_gate)           │
  └─────────────────┴────────────────────────────────────────────────────┘
```

## Beat Timing Math

```
  Given:
    BPM          = 120
    offset       = 0.200s
    beat_period  = 60.0 / 120 = 0.500s
    lead_beats   = 4
    lead_time    = 4 * 0.500 = 2.000s

  For beat N:

    beat_time(N)  = offset + N * beat_period
    spawn_time(N) = beat_time(N) - lead_time

  Example (beat 16):

    beat_time(16)  = 0.200 + 16 * 0.500 = 8.200s
    spawn_time(16) = 8.200 - 2.000      = 6.200s

  The obstacle spawns at t=6.200 at y=SPAWN_Y (-120),
  scrolls at 520 px/s, and arrives at y=PLAYER_Y (920)
  at t=8.200s, exactly on beat 16.

  ┌──────────────────────────────────────────────────────────┐
  │  PROOF:                                                  │
  │  travel_dist = PLAYER_Y - SPAWN_Y = 920 - (-120) = 1040 │
  │  travel_time = 1040 / 520 = 2.000s = lead_time  ✓       │
  └──────────────────────────────────────────────────────────┘
```

# ═══════════════════════════════════════════════════
# SECTION 2 — BPM-DERIVED CONSTANTS
# ═══════════════════════════════════════════════════

## All Game Constants Flow From BPM

```
  Given a song with BPM and lead_beats:

  ┌────────────────────────────────────────────────────────────────┐
  │                                                                │
  │  beat_period   = 60.0 / bpm                                    │
  │                                                                │
  │  lead_time     = lead_beats * beat_period                      │
  │                                                                │
  │  scroll_speed  = approach_dist / lead_time                     │
  │               = (PLAYER_Y - SPAWN_Y) / lead_time              │
  │               = 1040.0 / lead_time                             │
  │                                                                │
  │  window_dur    = max(BASE_WINDOW_BEATS * beat_period,          │
  │                      MIN_SHAPE_WINDOW)                         │
  │               = max(1.6 * beat_period, 0.36)                   │
  │                                                                │
  │  half_window   = window_dur / 2.0                              │
  │                                                                │
  │  morph_dur     = max(BASE_MORPH_BEATS * beat_period,           │
  │                      MIN_MORPH_DURATION)                        │
  │               = max(0.2 * beat_period, 0.08)                   │
  │                                                                │
  │  cooldown      = WINDOW_COOLDOWN  (fixed 0.05s)               │
  │                                                                │
  │  min_diff_gap  = window_dur + morph_dur * 2 + cooldown        │
  │               (in beats: ceil(min_diff_gap / beat_period))     │
  │                                                                │
  └────────────────────────────────────────────────────────────────┘
```

## Constants for `app/constants.h`

```cpp
// ── Rhythm Core ──────────────────────────────────
constexpr float APPROACH_DIST        = 1040.0f;  // |SPAWN_Y - PLAYER_Y|
constexpr int   DEFAULT_LEAD_BEATS   = 4;        // beats of scroll time

// ── Shape Window (BPM-derived, but with floors) ──
constexpr float BASE_WINDOW_BEATS    = 1.6f;     // window = 1.6 × beat_period
constexpr float MIN_SHAPE_WINDOW     = 0.36f;    // floor (never shorter)
constexpr float BASE_MORPH_BEATS     = 0.2f;     // morph = 0.2 × beat_period
constexpr float MIN_MORPH_DURATION   = 0.08f;    // morph floor
constexpr float WINDOW_COOLDOWN      = 0.05f;    // gap after window ends

// ── Timing Grade Thresholds (% of half-window from peak) ──
constexpr float GRADE_PERFECT_MAX    = 0.25f;    //  0% — 25%  = PERFECT
constexpr float GRADE_GOOD_MAX       = 0.50f;    // 25% — 50%  = GOOD
constexpr float GRADE_OK_MAX         = 0.75f;    // 50% — 75%  = OK
//                                                // 75% — 100% = BAD

// ── Timing Grade Score Multipliers ──────────────
constexpr float TIMING_MULT_PERFECT  = 1.50f;
constexpr float TIMING_MULT_GOOD     = 1.00f;
constexpr float TIMING_MULT_OK       = 0.50f;
constexpr float TIMING_MULT_BAD      = 0.25f;

// ── Window Scaling by Grade ─────────────────────
// Base window is calibrated for OK. Better grades
// scale down remaining active time so player
// recovers to Hexagon faster.
constexpr float WINDOW_SCALE_BAD     = 1.00f;    // full window
constexpr float WINDOW_SCALE_OK      = 1.00f;    // full window (base)
constexpr float WINDOW_SCALE_GOOD    = 0.75f;    // shortened tail
constexpr float WINDOW_SCALE_PERFECT = 0.50f;    // snaps shut

// ── Song / HP ────────────────────────────────────
constexpr int   MAX_HP               = 5;        // misses before song fails
constexpr int   HP_DRAIN_ON_MISS     = 1;        // HP lost per miss
constexpr int   HP_RECOVER_ON_PERFECT= 1;        // HP gained on PERFECT
```

## Removed / Replaced Constants

```
  ┌────────────────────────────────────────────────────────────────┐
  │  REMOVED from constants.h (no longer applicable):              │
  │                                                                │
  │  × SPEED_RAMP_RATE    — speed is BPM-derived, not time-ramped │
  │  × SPAWN_RAMP_RATE    — spawning is beat-map-driven           │
  │  × BURNOUT_SHRINK     — burnout zones are fixed, BPM scales  │
  │  × INITIAL_SPAWN_INT  — spawn timing comes from beat map      │
  │  × MIN_SPAWN_INT      — beat map controls density             │
  │  × BASE_SCROLL_SPEED  — replaced by BPM-derived scroll_speed │
  │                                                                │
  │  KEPT:                                                         │
  │  ✓ All burnout zone thresholds (ZONE_SAFE_MAX, etc.)          │
  │  ✓ All burnout multipliers (MULT_SAFE, etc.)                  │
  │  ✓ All scoring constants (PTS_*, CHAIN_BONUS)                 │
  │  ✓ All UI layout constants                                    │
  │  ✓ All input constants                                        │
  │  ✓ SPAWN_Y, DESTROY_Y, PLAYER_Y, PLAYER_SIZE                 │
  │  ✓ MORPH_DURATION stays as visual-only (BPM doesn't affect    │
  │    the animation interpolation speed, just the duration)       │
  └────────────────────────────────────────────────────────────────┘
```

# ═══════════════════════════════════════════════════
# SECTION 3 — ECS COMPONENTS
# ═══════════════════════════════════════════════════

## New Components

```cpp
// ═══════════════════════════════════════════════════
// SONG STATE — singleton, lives in registry context
// ═══════════════════════════════════════════════════

struct SongState {
    // ── Song metadata (loaded from beatmap.json) ──
    float bpm              = 120.0f;
    float offset           = 0.0f;    // seconds to first beat
    int   lead_beats       = 4;
    float duration_sec     = 180.0f;

    // ── Derived (computed once on song load) ──
    float beat_period      = 0.5f;    // 60.0 / bpm
    float lead_time        = 2.0f;    // lead_beats * beat_period
    float scroll_speed     = 520.0f;  // APPROACH_DIST / lead_time
    float window_duration  = 0.8f;    // max(BASE_WINDOW_BEATS * beat_period, MIN)
    float half_window      = 0.4f;    // window_duration / 2
    float morph_duration   = 0.1f;    // max(BASE_MORPH_BEATS * beat_period, MIN)

    // ── Playback state ──
    float song_time        = 0.0f;    // current position in seconds
    int   current_beat     = -1;      // most recent beat that has "ticked"
    bool  playing          = false;
    bool  finished         = false;

    // ── Beat schedule cursor ──
    size_t next_spawn_idx  = 0;       // index into sorted beat list
};


// ═══════════════════════════════════════════════════
// BEAT EVENT — per-obstacle, carries chart data
// ═══════════════════════════════════════════════════

struct BeatInfo {
    int   beat_index    = 0;      // which beat in the chart
    float arrival_time  = 0.0f;   // exact time obstacle should reach PLAYER_Y
    float spawn_time    = 0.0f;   // when this obstacle was actually spawned
};


// ═══════════════════════════════════════════════════
// LOADED BEAT MAP — singleton, the full chart in memory
// ═══════════════════════════════════════════════════

struct BeatEntry {
    int          beat_index  = 0;
    ObstacleKind kind        = ObstacleKind::ShapeGate;
    Shape        shape       = Shape::Circle;   // for shape/combo/split
    int8_t       lane        = 1;               // for shape/split
    uint8_t      blocked_mask= 0;               // for lane_block/combo
};

struct BeatMap {
    std::string          song_id;
    std::string          song_path;   // path to audio file
    float                bpm       = 120.0f;
    float                offset    = 0.0f;
    int                  lead_beats= 4;
    float                duration  = 180.0f;
    std::vector<BeatEntry> beats;             // sorted by beat_index

    // BPM is fixed for the entire song — no mid-song tempo changes.
};


// ═══════════════════════════════════════════════════
// HP STATE — singleton, songs are finite so we use HP
// ═══════════════════════════════════════════════════

struct HPState {
    int current = 5;    // MAX_HP
    int max_hp  = 5;
};


// ═══════════════════════════════════════════════════
// SONG RESULTS — singleton, accumulates during play
// ═══════════════════════════════════════════════════

struct SongResults {
    int perfect_count   = 0;
    int good_count      = 0;
    int ok_count        = 0;
    int bad_count       = 0;
    int miss_count      = 0;
    int max_chain       = 0;
    float best_burnout  = 0.0f;
};
```

## Modified Components

```cpp
// ═══════════════════════════════════════════════════
// Shape Enum (MODIFIED — add Hexagon)
// ═══════════════════════════════════════════════════

enum class Shape : uint8_t {
    Circle   = 0,
    Square   = 1,
    Triangle = 2,
    Hexagon  = 3    // NEW — default/neutral/rest
};


// ═══════════════════════════════════════════════════
// WindowPhase (NEW enum)
// ═══════════════════════════════════════════════════

enum class WindowPhase : uint8_t {
    Idle     = 0,   // player is Hexagon, no window active
    MorphIn  = 1,   // transitioning Hex → target shape
    Active   = 2,   // fully in target shape, scoring zones live
    MorphOut = 3    // transitioning target shape → Hex
};


// ═══════════════════════════════════════════════════
// PlayerShape (MODIFIED — add window tracking)
// ═══════════════════════════════════════════════════

struct PlayerShape {
    // --- existing fields (defaults changed) ---
    Shape current    = Shape::Hexagon;    // WAS: Circle
    Shape previous   = Shape::Hexagon;    // WAS: Circle
    float morph_t    = 1.0f;

    // --- new fields ---
    Shape       target_shape    = Shape::Hexagon;
    WindowPhase phase           = WindowPhase::Idle;
    float       window_timer    = 0.0f;   // elapsed since ACTIVE began
    float       window_start    = 0.0f;   // song_time when window triggered
    float       peak_time       = 0.0f;   // song_time of window peak

    // --- window scaling (set after collision graded) ---
    float       window_scale    = 1.0f;   // 1.0 = full, 0.5 = PERFECT snap
    bool        graded          = false;  // true once this window's obstacle scored
};


// ═══════════════════════════════════════════════════
// TimingGrade (NEW — emplaced on obstacle at collision)
// ═══════════════════════════════════════════════════

enum class TimingTier : uint8_t {
    Bad     = 0,    // ×0.25
    Ok      = 1,    // ×0.50
    Good    = 2,    // ×1.00
    Perfect = 3     // ×1.50
};

struct TimingGrade {
    TimingTier tier      = TimingTier::Bad;
    float      precision = 0.0f;  // 0.0 = edge, 1.0 = dead center
};


// ═══════════════════════════════════════════════════
// DifficultyConfig (MODIFIED — BPM-driven)
// ═══════════════════════════════════════════════════

struct DifficultyConfig {
    // REMOVED: speed_multiplier, spawn_interval, spawn_timer,
    //          burnout_window_scale, elapsed
    // These are now in SongState (BPM-derived)

    float scroll_speed         = 520.0f;  // = SongState.scroll_speed
    float window_duration      = 0.8f;    // = SongState.window_duration
    float morph_duration       = 0.1f;    // = SongState.morph_duration
    // Retained for compatibility — set once from SongState at song load
};
```

## Component Memory Layout

```
  ┌─────────────────────────────────────────────────────────────┐
  │  HOT (accessed every frame):                                 │
  │    Position         8B   ← obstacle scroll                  │
  │    Velocity         8B   ← obstacle scroll                  │
  │    PlayerShape     34B   ← window ticking + collision       │
  │                                                              │
  │  WARM (accessed on beat events):                             │
  │    BeatInfo        12B   ← spawn + timing grade calc        │
  │    Obstacle         4B   ← collision + scoring              │
  │    RequiredShape    1B   ← collision check                  │
  │    TimingGrade      5B   ← scoring (existential)           │
  │    ScoredTag        0B   ← scoring (existential tag)       │
  │                                                              │
  │  COLD (singleton, context):                                  │
  │    SongState       60B   ← beat scheduling                  │
  │    BeatMap        ~2KB   ← loaded once, read-only          │
  │    HPState          8B   ← miss tracking                    │
  │    SongResults     32B   ← grade accumulation              │
  │    ScoreState      24B   ← running score                   │
  │    BurnoutState    16B   ← burnout meter                   │
  │                                                              │
  │  Per-obstacle total: ~38B (Position+Velocity+BeatInfo+      │
  │                       Obstacle+RequiredShape)                │
  │  Peak entities: ~8 obstacles + 1 player = 9                  │
  │  Hot data: ~350B — fits in ONE cache line group              │
  └─────────────────────────────────────────────────────────────┘
```

# ═══════════════════════════════════════════════════
# SECTION 4 — SYSTEM PIPELINE CHANGES
# ═══════════════════════════════════════════════════

## New Pipeline

```
  ┌─ INPUT PHASE ──────────────────────────────────────────────┐
  │ input_system                                    [no change]│
  │ gesture_system                                  [no change]│
  └────────────────────────────────────────────────────────────┘
           ↓
  ┌─ STATE MACHINE ────────────────────────────────────────────┐
  │ game_state_system                               [no change]│
  └────────────────────────────────────────────────────────────┘
           ↓
  ┌─ RHYTHM ENGINE ────────────────────────────────── [NEW] ★ ─┐
  │ song_playback_system            ★ NEW                      │
  │   → advances song_time, fires beat ticks                   │
  │   → detects song end → transition to results               │
  │                                                            │
  │ beat_scheduler_system           ★ NEW                      │
  │   → walks beat map, spawns obstacles at spawn_time         │
  │   → replaces obstacle_spawn_system for charted songs       │
  └────────────────────────────────────────────────────────────┘
           ↓
  ┌─ PLAYER LOGIC ─────────────────────────────────────────────┐
  │ player_action_system                             [MOD]     │
  │   → starts shape window on button press                    │
  │   → computes peak_time from song_time + morph + half_window│
  │                                                            │
  │ shape_window_system              ★ NEW                     │
  │   → ticks window phases: Idle→MorphIn→Active→MorphOut→Idle │
  │   → reverts to Hexagon when window expires                 │
  │   → applies window_scale to remaining active time          │
  │     after collision is graded (GOOD/PERF = faster revert)  │
  │                                                            │
  │ player_movement_system                           [MOD]     │
  │   → ticks morph_t visual interpolation                     │
  └────────────────────────────────────────────────────────────┘
           ↓
  ┌─ WORLD LOGIC ──────────────────────────────────────────────┐
  │ scroll_system                                   [no change]│
  │   → moves obstacles by Velocity * dt                       │
  │                                                            │
  │ burnout_system                                  [no change]│
  │   → fills meter based on nearest obstacle proximity        │
  │                                                            │
  │ collision_system                                 [MOD]     │
  │   → checks shape match (now includes Hexagon → always fail)│
  │   → computes TimingGrade from peak_time vs collision time  │
  │   → sets PlayerShape.window_scale from grade               │
  │     (GOOD=0.75, PERFECT=0.50, else 1.0)                   │
  │   → on MISS: drains HP instead of instant game over        │
  │   → emplaces TimingGrade + ScoredTag on cleared obstacles  │
  └────────────────────────────────────────────────────────────┘
           ↓
  ┌─ SCORING & CLEANUP ───────────────────────────────────────-┐
  │ scoring_system                                   [MOD]     │
  │   → reads TimingGrade, computes timing_mult × burnout_mult │
  │   → updates SongResults counters                           │
  │   → spawns grade popup VFX                                 │
  │                                                            │
  │ hp_system                        ★ NEW                     │
  │   → checks HPState, triggers song fail if HP <= 0          │
  │                                                            │
  │ lifetime_system                                 [no change]│
  │ particle_system                                 [no change]│
  │ cleanup_system                                  [no change]│
  └────────────────────────────────────────────────────────────┘
           ↓
  ┌─ RENDER ───────────────────────────────────────────────────┐
  │ render_system                                    [MOD]     │
  │   → renders Hexagon shape                                  │
  │   → renders beat-sync hex pulse                            │
  │   → renders timing grade popups (PERFECT/GOOD/OK/BAD)     │
  │   → renders HP bar                                         │
  │   → renders song progress bar                              │
  └────────────────────────────────────────────────────────────┘
           ↓
  ┌─ AUDIO ────────────────────────────────────────────────────┐
  │ audio_system                                     [MOD]     │
  │   → plays song audio (raylib audio)                          │
  │   → syncs song_time with audio playback position           │
  │   → plays SFX on grade events                              │
  └────────────────────────────────────────────────────────────┘
```

## New System Declarations (for `all_systems.h`)

```cpp
// Phase 3.5: Rhythm Engine (NEW)
void song_playback_system(entt::registry& reg, float dt);
void beat_scheduler_system(entt::registry& reg, float dt);

// Phase 3.7: Shape Window (NEW)
void shape_window_system(entt::registry& reg, float dt);

// Phase 5.5: HP (NEW)
void hp_system(entt::registry& reg, float dt);
```

## System Data Flow (One Beat)

```
  FRAME WHERE A BEAT EVENT OCCURS:

  ┌─────────────────────────────────────────────────────────────┐
  │ song_playback_system                                        │
  │   reads: audio playback position                            │
  │   writes: SongState.song_time, SongState.current_beat       │
  │   fires: beat_tick event (for hex pulse VFX)                │
  └──────────────────────────┬──────────────────────────────────┘
                             │
  ┌──────────────────────────┴──────────────────────────────────┐
  │ beat_scheduler_system                                       │
  │   reads: SongState.song_time, BeatMap.beats[next_spawn_idx] │
  │   if song_time >= beats[i].spawn_time:                      │
  │     creates obstacle entity with BeatInfo, Position,         │
  │     Velocity(scroll_speed), RequiredShape, Obstacle          │
  │   writes: SongState.next_spawn_idx++                        │
  └──────────────────────────┬──────────────────────────────────┘
                             │
          (obstacle scrolls for lead_time seconds)
                             │
  ┌──────────────────────────┴──────────────────────────────────┐
  │ collision_system (when obstacle reaches PLAYER_Y)           │
  │   reads: PlayerShape.current, PlayerShape.peak_time         │
  │          RequiredShape.shape, SongState.song_time            │
  │   computes: pct = |song_time - peak_time| / half_window     │
  │   writes: TimingGrade on obstacle, ScoredTag                │
  │           PlayerShape.window_scale (from grade tier)        │
  │           PlayerShape.graded = true                         │
  │   on MISS: HPState.current -= HP_DRAIN_ON_MISS              │
  └──────────────────────────────────────────────────────────────┘
                             │
  ┌──────────────────────────┴──────────────────────────────────┐
  │ shape_window_system                                         │
  │   reads: PlayerShape.window_timer, window_scale, graded     │
  │   if graded && phase == Active:                             │
  │     remaining_active *= window_scale                        │
  │     (GOOD/PERFECT → faster transition to MorphOut)          │
  │   writes: PlayerShape.phase, window_timer                   │
  └──────────────────────────┬──────────────────────────────────┘
                             │
  ┌──────────────────────────┴──────────────────────────────────┐
  │ scoring_system                                              │
  │   reads: TimingGrade.tier, BurnoutState.zone, Obstacle.pts  │
  │   computes: final = base × timing_mult × burnout_mult       │
  │   writes: ScoreState.score, SongResults.perfect_count++     │
  │   spawns: ScorePopup entity with grade text                 │
  └─────────────────────────────────────────────────────────────┘
```

## obstacle_spawn_system — What Happens to It?

```
  ┌────────────────────────────────────────────────────────────────┐
  │  obstacle_spawn_system is NOT deleted. It is BYPASSED when    │
  │  playing a charted song.                                       │
  │                                                                │
  │  if (SongState.playing) {                                      │
  │      // beat_scheduler_system handles spawning                 │
  │      return;                                                   │
  │  }                                                             │
  │  // else: legacy random spawning for practice/sandbox mode    │
  │                                                                │
  │  This preserves backward compatibility and allows a "free     │
  │  play" mode without a beat map.                                │
  └────────────────────────────────────────────────────────────────┘
```

# ═══════════════════════════════════════════════════
# SECTION 5 — EDGE CASES
# ═══════════════════════════════════════════════════

## Rapid Beats (Consecutive Obstacles on Adjacent Beats)

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │  CASE: Two obstacles on beats 16 and 17 (0.5s apart at 120 BPM)   │
  ├─────────────────────────────────────────────────────────────────────┤
  │                                                                     │
  │  SUB-CASE A: Same shape (both ▲)                                   │
  │  ─────────────────────────────────                                  │
  │  One press covers both. Each gate gets its own TimingGrade          │
  │  based on where in the window it was hit. The window is             │
  │  0.80s long; two gates 0.50s apart both fit.                        │
  │                                                                     │
  │  beat 16           beat 17                                          │
  │    ▲gate1            ▲gate2                                         │
  │      │   0.5s gap      │                                            │
  │  ──╱▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲╲──                                      │
  │    │ gate1 = GOOD │ gate2 = PERFECT │                               │
  │                                                                     │
  │  SUB-CASE B: Different shapes (▲ then ●)                           │
  │  ──────────────────────────────────────                             │
  │  Player must: press ▲ → window plays → reverts to ⬡ → press ●     │
  │  Minimum turnaround = morph_out(0.10) + cooldown(0.05) +           │
  │                        morph_in(0.10) = 0.25s                       │
  │  Gap available = 0.50s. Feasible but tight.                         │
  │                                                                     │
  │  ──╱▲▲▲▲╲──⬡──╱●●●●╲──                                            │
  │    │ short  │cd│ short │                                            │
  │                                                                     │
  │  BEAT MAP AUTHORING RULE:                                           │
  │  Different-shape obstacles must be ≥ 3 beats apart at any BPM.     │
  │  The beat_map_validator enforces this.                               │
  └─────────────────────────────────────────────────────────────────────┘
```

## Beat Drops (Silence/Breakdown Sections)

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │  CASE: Song has a breakdown where the beat stops (e.g., buildup)   │
  ├─────────────────────────────────────────────────────────────────────┤
  │                                                                     │
  │  The beat map simply has NO entries for those beats.                │
  │  No obstacles spawn. Player rests as ⬡.                            │
  │  Hex continues pulsing on the underlying BPM (musical silence      │
  │  ≠ BPM stopping).                                                  │
  │                                                                     │
  │  beats: [ ...beat 48, beat 49, ────── gap ──────, beat 64, ...]    │
  │                                                                     │
  │  This creates tension: "When does the drop hit??"                   │
  │  Answer: beat 64. The beat map author placed it there.              │
  │  The player who has practiced KNOWS when it comes.                  │
  └─────────────────────────────────────────────────────────────────────┘
```

## Player Presses Wrong Shape

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │  CASE: ● gate approaching, player presses ▲                       │
  ├─────────────────────────────────────────────────────────────────────┤
  │                                                                     │
  │  Window opens for ▲. Gate requires ●. When gate arrives:           │
  │    PlayerShape.current = Triangle ≠ Circle → MISS 💀               │
  │                                                                     │
  │  This is NOT a timing issue — it's a "wrong note" issue.           │
  │  The player saw ● but pressed ▲.                                   │
  │                                                                     │
  │  Result: HP drain, chain reset, "MISS" popup                       │
  │                                                                     │
  │  The window still plays out normally (you committed to ▲).         │
  │  If you realize mid-window, pressing ● will INTERRUPT              │
  │  (cancel ▲, start ● — see rhythm-design.md §5, Case 2).           │
  │  You might still recover if you're fast enough.                     │
  └─────────────────────────────────────────────────────────────────────┘
```

## Obstacle Passes Without Any Shape Press

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │  CASE: Player is ⬡ Hexagon when ShapeGate arrives                  │
  ├─────────────────────────────────────────────────────────────────────┤
  │                                                                     │
  │  ⬡ ≠ any required shape → MISS 💀                                 │
  │  HP drain. Chain reset. "MISS" popup.                               │
  │                                                                     │
  │  Unlike the old endless runner (instant game over),                │
  │  a MISS drains 1 HP. Player has MAX_HP = 5 total.                  │
  │  At 0 HP, the song ends early (FAIL).                              │
  │                                                                     │
  │  This is critical: songs are 2-3 minutes long. Instant death       │
  │  on one miss would be too punishing for a SONG-length level.       │
  └─────────────────────────────────────────────────────────────────────┘
```

## HP and Song Failure vs Song Completion

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │  HP SYSTEM (NEW — replaces instant game-over for shape misses)     │
  ├─────────────────────────────────────────────────────────────────────┤
  │                                                                     │
  │  HP BAR:   ♥ ♥ ♥ ♥ ♥   (5 hearts)                                │
  │                                                                     │
  │  On MISS:     HP -= 1   ♥ ♥ ♥ ♥ ♡                                │
  │  On PERFECT:  HP += 1   ♥ ♥ ♥ ♥ ♥  (capped at MAX_HP)            │
  │                                                                     │
  │  HP = 0:      SONG FAIL                                            │
  │     → "Song Failed" screen (not "Game Over")                       │
  │     → Shows partial results + "Retry?" button                      │
  │                                                                     │
  │  Song reaches duration_sec:  SONG COMPLETE                         │
  │     → Results screen with grade (S/A/B/C/D/F)                      │
  │                                                                     │
  │  NON-SHAPE OBSTACLES (LaneBlock, LowBar, HighBar):                │
  │     → These are still instant-fail (dodge or die).                 │
  │     → They happen regardless of shape (⬡ can dodge).              │
  │     → Missing a dodge = same HP drain.                             │
  │                                                                     │
  │  RATIONALE: In rhythm games, the standard is HP drain, not         │
  │  instant death. This allows players to recover from mistakes       │
  │  while still maintaining stakes. PERFECT healing creates a         │
  │  "clutch recovery" narrative arc within each song.                 │
  └─────────────────────────────────────────────────────────────────────┘
```

## Window Expiry Same Frame as Collision

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │  CASE: Window expires on the exact frame the obstacle arrives      │
  ├─────────────────────────────────────────────────────────────────────┤
  │  RULING: collision_system runs BEFORE shape_window_system.         │
  │  Collision resolves first → player still in Active phase → PASS.  │
  │  Then window reverts. No race condition.                           │
  │                                                                     │
  │  If the timing grade puts it at >100% from peak, it's a BAD.      │
  │  Still better than a MISS.                                         │
  └─────────────────────────────────────────────────────────────────────┘
```

## Same-Shape Spam / Different-Shape Interrupt

```
  (Unchanged from rhythm-design.md §5 — see Cases 1 and 2 there)

  Same-shape spam:     Ignored during active window or cooldown.
  Different-shape mid: INTERRUPT. Current window cancelled, new
                       window begins fresh. Punishes indecision.
```

## Song Audio Desync

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │  CASE: Audio playback drifts from game timer                       │
  ├─────────────────────────────────────────────────────────────────────┤
  │                                                                     │
  │  song_playback_system ALWAYS uses the audio engine's position      │
  │  as the source of truth. Game timer is slaved to audio.            │
  │                                                                     │
  │  Every frame:                                                       │
  │    song_time = GetMusicTimePlayed()  // or equivalent             │
  │                                                                     │
  │  If audio position is unavailable (some backends don't support     │
  │  querying position), fall back to:                                 │
  │    song_time += dt                                                 │
  │    // with periodic re-sync if audio position becomes available    │
  │                                                                     │
  │  CRITICAL: obstacle spawn_time and arrival_time are computed from  │
  │  the same song_time, so any drift affects ALL obstacles equally.   │
  │  Relative timing (what matters for grades) is preserved.           │
  └─────────────────────────────────────────────────────────────────────┘
```

---

## Appendix A — Implementation Phases

```
  PHASE 1 — RHYTHM ENGINE FOUNDATION
  ══════════════════════════════════════
  ┌────────────────────────────────────────────┐
  │ • beatmap.json loader + validator          │
  │ • SongState + BeatMap components           │
  │ • song_playback_system (song_time ticker)  │
  │ • beat_scheduler_system (spawn on beat)    │
  │ • Verify: obstacles arrive on-beat         │
  └────────────────────────────────────────────┘
           │
           ▼
  PHASE 2 — SHAPE WINDOW (from rhythm-design.md §5)
  ══════════════════════════════════════════════
  ┌────────────────────────────────────────────┐
  │ • Hexagon shape + WindowPhase              │
  │ • shape_window_system                      │
  │ • player_action_system starts windows      │
  │ • peak_time calculation from song_time     │
  └────────────────────────────────────────────┘
           │
           ▼
  PHASE 3 — TIMING GRADES + COMBINED SCORING
  ══════════════════════════════════════════════
  ┌────────────────────────────────────────────┐
  │ • TimingGrade component + collision calc   │
  │ • scoring_system reads timing × burnout    │
  │ • SongResults accumulation                 │
  │ • Grade popup VFX                          │
  └────────────────────────────────────────────┘
           │
           ▼
  PHASE 4 — HP + SONG COMPLETION
  ══════════════════════════════════════════════
  ┌────────────────────────────────────────────┐
  │ • HPState component + hp_system            │
  │ • MISS → HP drain (not instant game over)  │
  │ • PERFECT → HP recovery                    │
  │ • Song complete → results screen           │
  │ • Song fail (HP=0) → fail screen           │
  └────────────────────────────────────────────┘
           │
           ▼
  PHASE 5 — AUDIO INTEGRATION
  ══════════════════════════════════════════════
  ┌────────────────────────────────────────────┐
  │ • raylib audio song playback               │
  │ • Audio position → song_time sync          │
  │ • Hex beat-pulse VFX synced to BPM         │
  │ • Grade-specific SFX (PERFECT chime, etc.) │
  └────────────────────────────────────────────┘
           │
           ▼
  PHASE 6 — POLISH + EDGE CASES
  ══════════════════════════════════════════════
  ┌────────────────────────────────────────────┐
  │ • Beat map generator tool (aubio pipeline) │
  │ • beat_map_validator CLI tool              │
  │ • Tutorial song (80 BPM, gentle intro)     │
  │ • Debug overlay: beat timeline, window vis │
  │ • Song select screen (future)              │
  └────────────────────────────────────────────┘
```

---

*This document specifies the technical implementation for the rhythm-core mechanic.
For game design rationale, player experience goals, and design rules,
see `rhythm-design.md`.*
