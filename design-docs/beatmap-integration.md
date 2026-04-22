# SHAPESHIFTER — Beatmap & Audio Integration Spec
## Dev Spec · v2.0

> **Depends on:** `rhythm-spec.md` (component shapes, system signatures), `rhythm-design.md` (design rationale)
>
> **Inputs consumed:**
> - `content/beatmaps/<song_id>_beatmap.json` — authored obstacle timeline
> - `content/audio/<song_id>.flac` — song audio file
>
> **Scope:** Load a beatmap JSON + WAV into the running game, replace random spawning with beat-driven spawning, and synchronise audio playback to `song_time`. This spec covers the data layouts, system transforms, build pipeline changes, and audio integration via raylib.

```
  ┌──────────────────────────────────────────────────────────────────┐
  │  CURRENT STATE (main branch — raylib migration complete)         │
  │                                                                  │
  │  ✅ raylib for rendering, input, windowing                      │
  │  ✅ nlohmann-json in vcpkg.json                                 │
  │  ✅ BeatMap, SongState, SongResults in rhythm.h                 │
  │  ✅ beat_map_loader.h/.cpp — JSON → BeatMap with validation     │
  │  ✅ song_playback_system — advances song_time via += dt         │
  │  ✅ beat_scheduler_system — spawns obstacles from BeatMap       │
  │  ✅ obstacle_spawn_system — guards on SongState.playing         │
  │  ✅ shape_window_system — timing tier grading                   │
  │  ✅ hp_system — miss tracking                                   │
  │  ❌ audio_system: stub (clears AudioQueue, no actual playback)  │
  │  ❌ No content/ asset copy rules in CMakeLists.txt              │
  │  ❌ No InitAudioDevice / LoadMusicStream / playback code        │
  │  ❌ song_playback_system uses += dt, not GetMusicTimePlayed()   │
  │                                                                  │
  │  TARGET STATE (after this spec)                                  │
  │                                                                  │
  │  • audio_system: streams WAV via raylib, plays SFX              │
  │  • song_playback_system: syncs song_time to audio position      │
  │  • content/ copied to build dir by CMake                        │
  │  • InitAudioDevice + LoadMusicStream in main.cpp                │
  └──────────────────────────────────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 1 — DATA: WHAT FLOWS IN
# ═══════════════════════════════════════════════════

## 1.1 Beatmap JSON (authored file)

```
  content/beatmaps/2_drama_beatmap.json
  ~3KB, loaded once at game start, never re-read.
```

**Schema** (matches rhythm-spec.md §1):

```json
{
  "song_id":      "2_drama",
  "title":        "2_drama",
  "bpm":          119.64,
  "offset":       1.813,
  "lead_beats":   4,
  "duration_sec": 199.35,
  "difficulties": {
    "easy":   { "beats": [ /* obstacle array */ ], "count": 73 },
    "medium": { "beats": [ /* obstacle array */ ], "count": 149 },
    "hard":   { "beats": [ /* obstacle array */ ], "count": 256 }
  },
  "structure": [
    { "section": "verse", "start": 0.0, "end": 143.17, "intensity": "medium" },
    ...
  ]
}
```

**Obstacle object variants:**

```json
// shape_gate — most common (~85% of obstacles)
{ "beat": 12, "kind": "shape_gate", "shape": "circle", "lane": 0 }

// low_bar — jump over
{ "beat": 350, "kind": "low_bar" }

// high_bar — slide under
{ "beat": 366, "kind": "high_bar" }

// lane_push_left — passive push, player auto-shifts left
{ "beat": 42, "kind": "lane_push_left", "lane": 1 }

// lane_push_right — passive push, player auto-shifts right
{ "beat": 44, "kind": "lane_push_right", "lane": 0 }
```

**Key field: `beat` is an INDEX, not a timestamp.** The game resolves `beat` → time via: `offset + beat × (60.0 / bpm)`.

## 1.2 Audio WAV

```
  content/audio/2_drama.flac
  ~41 MB, 16-bit stereo, 44100 Hz, 241.13 seconds.
  Streamed at runtime via raylib LoadMusicStream.
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 2 — ECS DATA LAYOUT
# ═══════════════════════════════════════════════════

## 2.1 Singletons (reg.ctx()) — ALREADY IMPLEMENTED

All three are defined in `app/components/rhythm.h` on `main`. Cold data — read a few
times per frame, not iterated in bulk.

```cpp
// Already in rhythm.h — BeatEntry is 8 bytes per obstacle
struct BeatEntry {
    int          beat_index   = 0;       //  4B
    ObstacleKind kind         = ObstacleKind::ShapeGate;  // 1B
    Shape        shape        = Shape::Circle;            // 1B
    int8_t       lane         = 1;       //  1B
    uint8_t      blocked_mask = 0;       //  1B
};

struct BeatMap {
    std::string song_id;
    std::string title;
    std::string song_path;
    float       bpm        = 120.0f;
    float       offset     = 0.0f;
    int         lead_beats = 4;
    float       duration   = 180.0f;
    std::string difficulty;
    std::vector<BeatEntry> beats;
};

struct SongState {
    float bpm, offset, beat_period, lead_time, scroll_speed;
    float window_duration, half_window, morph_duration;
    float  song_time     = 0.0f;
    int    current_beat  = -1;
    bool   playing       = false;
    bool   finished      = false;
    size_t next_spawn_idx = 0;
};

struct SongResults {
    int perfect_count, good_count, ok_count, bad_count, miss_count;
    int max_chain;
    float best_burnout;
    int total_notes;
};
```

```
  Memory: BeatEntry = 8 bytes × 256 (hard) = 2 KB. Fits in L1.
  SongState ~64 bytes. SongResults ~32 bytes. One cache line each.
```

### 2.1.1 NEW: MusicContext (singleton for audio state)

```cpp
// ── To be added to rhythm.h or a new app/components/music.h ──

struct MusicContext {
    Music  stream  = {};       // raylib Music handle (streaming)
    bool   loaded  = false;    // true if LoadMusicStream succeeded
    bool   started = false;    // true after first PlayMusicStream call
    float  volume  = 0.8f;    // master music volume [0.0, 1.0]
};
```

This is the only new data structure needed. It holds the raylib `Music` handle
and playback state. Emplaced in `reg.ctx()` alongside SongState.

## 2.2 Existing Components — No Changes Needed

The current obstacle components already match the beatmap schema:

```
  ObstacleKind::ShapeGate      → "shape_gate"       ✓  already exists
  ObstacleKind::LaneBlock      → "lane_block"       ✓  legacy/backward-compatible support
  ObstacleKind::LowBar         → "low_bar"          ✓  already exists
  ObstacleKind::HighBar        → "high_bar"         ✓  already exists
  ObstacleKind::LanePushLeft   → "lane_push_left"   ✓  already exists
  ObstacleKind::LanePushRight  → "lane_push_right"  ✓  already exists

  RequiredShape { shape }    → beatmap "shape" field    ✓
  RequiredVAction { action } → low_bar/high_bar         ✓
```

**No new per-entity components required.** The beat_scheduler creates the same entity archetypes that obstacle_spawn_system already creates.

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 3 — SYSTEM TRANSFORMS
# ═══════════════════════════════════════════════════

## 3.1 Data Flow Diagram

```
  ┌──────────────────────────────────────────────────────────────────┐
  │  FRAME PIPELINE (on main branch — raylib)                       │
  │                                                                  │
  │  input_system            (polls raylib input → InputState)      │
  │       │                                                          │
  │  gesture_system          (InputState → GestureResult)           │
  │  game_state_system       (phase transitions)                    │
  │  ✅ song_playback_system (advances SongState.song_time)         │
  │  ★ beat_scheduler_system (SongState + BeatMap → spawn entities) │
  │  player_action_system    (GestureResult → PlayerShape, Lane)    │
  │  shape_window_system     (timing window morphing)               │
  │  player_movement_system  (animate Position from Lane/VState)    │
  │  difficulty_system       (elapsed → speed/spawn/burnout ramp)   │
  │  obstacle_spawn_system   (SKIPPED when SongState.playing)       │
  │       │                                                          │
  │  scroll_system           (Position += Velocity × dt)            │
  │  burnout_system          (nearest obstacle → BurnoutState)      │
  │  collision_system        (player vs obstacles → ScoredTag)      │
  │  scoring_system          (ScoredTag → ScoreState, popups)       │
  │  hp_system               (miss tracking → game over)            │
  │  lifetime_system         (Lifetime → destroy expired)           │
  │  particle_system         (ParticleData → shrink, gravity)       │
  │  cleanup_system          (destroy off-screen obstacles)         │
  │       │                                                          │
  │  render_system           (draw everything)                      │
  │  ★ audio_system          (raylib: stream music, play SFX)      │
  └──────────────────────────────────────────────────────────────────┘

  ✅ = already implemented    ★ = needs audio integration
```

## 3.2 beat_map_loader — ✅ ALREADY IMPLEMENTED

```
  app/beat_map_loader.h / .cpp — on main branch.
  Loads JSON → BeatMap, validates, inits SongState.
  Uses nlohmann-json. Called once at startup.
```

## 3.3 song_playback_system — ✅ IMPLEMENTED, NEEDS AUDIO SYNC

```
  Currently: song_time += dt (pure accumulator, drifts from audio)
  Target:    song_time = GetMusicTimePlayed(music) (authoritative)
```

**Modification needed:** When MusicContext is loaded and playing, use
`GetMusicTimePlayed()` as the authoritative clock instead of accumulation.
This eliminates drift between audio and gameplay.

```cpp
void song_playback_system(entt::registry& reg, float dt) {
    if (reg.ctx().get<GameState>().phase != GamePhase::Playing) return;

    auto* song  = reg.ctx().find<SongState>();
    auto* music = reg.ctx().find<MusicContext>();
    if (!song || !song->playing || song->finished) return;

    // Use audio position as authoritative clock when available
    if (music && music->loaded && music->started) {
        UpdateMusicStream(music->stream);
        song->song_time = GetMusicTimePlayed(music->stream);
    } else {
        song->song_time += dt;  // fallback: silent mode
    }

    // Update current beat
    if (song->beat_period > 0.0f) {
        int beat = static_cast<int>((song->song_time - song->offset) / song->beat_period);
        if (beat > song->current_beat && song->song_time >= song->offset) {
            song->current_beat = beat;
        }
    }

    // Detect song end
    if (song->song_time >= song->duration_sec) {
        song->finished = true;
        song->playing  = false;
    }
}
```

## 3.4 beat_scheduler_system — ✅ ALREADY IMPLEMENTED

```
  Spawns obstacle entities from BeatMap.beats[] when song_time
  crosses spawn_time thresholds. Same entity archetypes as random
  spawner — collision, scoring, render all work unchanged.
```

## 3.5 obstacle_spawn_system — ✅ ALREADY GUARDED

```
  Guards on SongState.playing — random spawning skipped when
  beatmap is active. Falls back to random when no beatmap loaded.
```

## 3.6 audio_system — NEEDS IMPLEMENTATION (raylib)

```
  Input:  AudioQueue (SFX events from other systems)
          MusicContext (singleton, loaded music stream)
  Output: raylib audio calls (music streaming + SFX playback)
```

```
  ┌──────────────────────────────────────────────────────────────┐
  │  Audio Architecture (raylib)                                 │
  │                                                              │
  │  Song:  LoadMusicStream(song_path) → PlayMusicStream(music)  │
  │         Loaded once. raylib streams from disk.               │
  │         Position sync: GetMusicTimePlayed(music)             │
  │         Must call UpdateMusicStream(music) every frame.      │
  │                                                              │
  │  SFX:   LoadSound(path) per SFX enum at init.               │
  │         PlaySound(sound) per queued SFX.                     │
  │         Channel allocation automatic.                        │
  │                                                              │
  │  Init:  InitAudioDevice()                                    │
  │         Called once in main.cpp before main loop.            │
  │                                                              │
  │  Shutdown: UnloadMusicStream + UnloadSound × N               │
  │            CloseAudioDevice()                                │
  └──────────────────────────────────────────────────────────────┘
```

```cpp
// ── app/systems/audio_system.cpp (rewritten) ────────────────

void audio_system(entt::registry& reg) {
    auto& queue = reg.ctx().get<AudioQueue>();

    // Play queued SFX
    // TODO: Load Sound handles at init, map SFX enum → Sound
    // for (int i = 0; i < queue.count; ++i) {
    //     PlaySound(sfx_sounds[static_cast<int>(queue.queue[i])]);
    // }

    audio_clear(queue);
}
```

**Key difference from SDL_mixer:** raylib's `UpdateMusicStream()` must be called
every frame to feed the audio buffer. This is done in `song_playback_system`
(not audio_system) because it's tightly coupled to song_time updates.

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 4 — BUILD PIPELINE CHANGES
# ═══════════════════════════════════════════════════

## 4.1 Dependencies — ✅ ALREADY IN PLACE

`main` branch vcpkg.json already includes:

```json
{
  "dependencies": [
    "entt",
    "raylib",
    "nlohmann-json",
    { "name": "catch2", "platform": "!linux" }
  ]
}
```

raylib includes full audio support (raudio module) — no additional audio
library needed. `InitAudioDevice()`, `LoadMusicStream()`, `PlayMusicStream()`,
`UpdateMusicStream()`, `GetMusicTimePlayed()` are all part of raylib.

## 4.2 CMakeLists.txt — NEEDS CONTENT COPY RULES

The existing CMakeLists.txt links raylib and nlohmann-json. Only missing piece
is copying content/ assets to the build directory.

## 4.3 Content Asset Copying

Mirror the existing font copying pattern:

```cmake
# ── Copy beatmap JSON + audio assets ─────────────────────────
file(GLOB BEATMAP_FILES ${CMAKE_SOURCE_DIR}/content/beatmaps/*.json)
file(GLOB AUDIO_FILES   ${CMAKE_SOURCE_DIR}/content/audio/*.flac)

if(BEATMAP_FILES)
    add_custom_command(TARGET shapeshifter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:shapeshifter>/content/beatmaps"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${BEATMAP_FILES}
            "$<TARGET_FILE_DIR:shapeshifter>/content/beatmaps/"
        COMMENT "Copying beatmap assets"
    )
endif()

if(AUDIO_FILES)
    add_custom_command(TARGET shapeshifter POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:shapeshifter>/content/audio"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${AUDIO_FILES}
            "$<TARGET_FILE_DIR:shapeshifter>/content/audio/"
        COMMENT "Copying audio assets"
    )
endif()
```

## 4.4 Source Files Status

```
  ✅ app/components/rhythm.h             — BeatMap, SongState, SongResults, etc.
  ✅ app/beat_map_loader.h / .cpp        — JSON → BeatMap with validation
  ✅ app/systems/song_playback_system.cpp — song_time advancement
  ✅ app/systems/beat_scheduler_system.cpp — obstacle spawning from BeatMap
  ✅ app/systems/shape_window_system.cpp  — timing window morphing
  ✅ app/systems/hp_system.cpp            — miss tracking
  ★  app/systems/audio_system.cpp         — NEEDS: raylib music streaming + SFX
  ★  app/components/rhythm.h (or music.h) — NEEDS: MusicContext struct
  ★  app/main.cpp                         — NEEDS: InitAudioDevice, LoadMusicStream
```

`audio_system.cpp` is already in `app/systems/` so it's auto-included in
`shapeshifter_lib` via the `file(GLOB)` rule. No CMake changes for source files.

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 5 — main.cpp INTEGRATION
# ═══════════════════════════════════════════════════

## 5.1 Singleton Init — ✅ MOSTLY DONE

Beat map loading is already in main.cpp on `main`. BeatMap, SongState,
SongResults, HPState are all emplaced. Only addition needed:

```cpp
// After beat map loading, load the music stream:
auto& mc = reg.ctx().emplace<MusicContext>();
if (has_beatmap) {
    auto& bm = reg.ctx().get<BeatMap>();
    mc.stream = LoadMusicStream(bm.song_path.c_str());
    if (IsMusicReady(mc.stream)) {
        mc.loaded = true;
        SetMusicVolume(mc.stream, mc.volume);
        TraceLog(LOG_INFO, "Music loaded: %s", bm.song_path.c_str());
    } else {
        TraceLog(LOG_WARNING, "Failed to load music: %s", bm.song_path.c_str());
    }
}
```

## 5.2 Audio Init — NEEDS ADDING

```cpp
// After InitWindow(), before main loop:
InitAudioDevice();

// Verify:
if (!IsAudioDeviceReady()) {
    TraceLog(LOG_WARNING, "Audio device not available — running silent");
}
```

## 5.3 Music Playback Start

Music should start when gameplay begins (GamePhase transitions to Playing):

```cpp
// In game_state_system or main loop, when transitioning to Playing:
auto* mc = reg.ctx().find<MusicContext>();
if (mc && mc->loaded && !mc->started) {
    PlayMusicStream(mc->stream);
    mc->started = true;
}
```

## 5.4 System Call Order — ✅ ALREADY CORRECT ON MAIN

The main loop on `main` already has the correct system order:

```cpp
song_playback_system(reg, FIXED_DT);     // advances song_time
beat_scheduler_system(reg, FIXED_DT);     // spawns from beatmap
// ... all other systems ...
audio_system(reg);                         // SFX playback
```

## 5.5 Shutdown

```cpp
// Before CloseWindow():
auto* mc = reg.ctx().find<MusicContext>();
if (mc && mc->loaded) {
    StopMusicStream(mc->stream);
    UnloadMusicStream(mc->stream);
}
CloseAudioDevice();
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 6 — IMPLEMENTATION ORDER
# ═══════════════════════════════════════════════════

Ordered by dependency chain. Steps marked ✅ are already on `main`.

```
  STEP 1 — Dependencies + Build                    ✅ DONE
  ─────────────────────────────
  • raylib + nlohmann-json already in vcpkg.json
  • CMakeLists.txt already finds and links both
  • raylib includes audio module (raudio) — no extra dep needed

  STEP 2 — Data Components                         ✅ DONE
  ─────────────────────────────
  • BeatMap, BeatEntry, SongState, SongResults, HPState in rhythm.h
  • TimingGrade, WindowPhase, BeatInfo all defined

  STEP 3 — Beat Map Loader                         ✅ DONE
  ─────────────────────────────
  • beat_map_loader.h/.cpp: JSON → BeatMap with validation
  • Handles all obstacle kinds including ComboGate, SplitPath
  • init_song_state() computes derived fields from BPM

  STEP 4 — Song Playback + Beat Scheduler          ✅ DONE
  ──────────────────────────────
  • song_playback_system: advances song_time, detects song end
  • beat_scheduler_system: spawns obstacles from BeatMap.beats[]
  • obstacle_spawn_system: guards on SongState.playing

  STEP 5 — Content Asset Copying                    ★ TODO
  ────────────────────────────────
  • Add content/beatmaps/*.json + content/audio/*.flac copy rules
  • Mirror existing font copy pattern in CMakeLists.txt
  • Verify: beatmap + WAV appear next to executable after build

  STEP 6 — Audio Playback (raylib)                  ★ TODO
  ──────────────────────────────────
  • Add MusicContext struct (raylib Music handle + state flags)
  • Add InitAudioDevice() in main.cpp (before main loop)
  • LoadMusicStream() when beatmap is loaded
  • PlayMusicStream() when gameplay starts (Playing phase)
  • UpdateMusicStream() + GetMusicTimePlayed() in song_playback_system
  • CloseAudioDevice() + UnloadMusicStream() at shutdown
  • Verify: music plays, obstacles sync to audible beats

  STEP 7 — Validation                               ★ TODO
  ────────────────────
  • Play the easy level end-to-end
  • Verify obstacle timing matches audible beats
  • Verify game over on miss still works
  • Verify fallback to random spawning when no beatmap
  • Verify game runs silently when WAV is missing
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 7 — EDGE CASES & RULINGS
# ═══════════════════════════════════════════════════

```
  ┌────────────────────────────────────┬──────────────────────────────────────┐
  │  Situation                         │  Ruling                              │
  ├────────────────────────────────────┼──────────────────────────────────────┤
  │  Beatmap JSON missing or corrupt   │  load_beat_map returns false.        │
  │                                    │  SongState not emplaced.             │
  │                                    │  Game falls back to random spawning. │
  ├────────────────────────────────────┼──────────────────────────────────────┤
  │  WAV file missing                  │  IsMusicReady() returns false.       │
  │                                    │  MusicContext.loaded = false.        │
  │                                    │  Game runs silently.                 │
  │                                    │  Obstacles still spawn on beat.      │
  ├────────────────────────────────────┼──────────────────────────────────────┤
  │  Audio/gameplay drift              │  GetMusicTimePlayed() is the         │
  │                                    │  authoritative clock. song_time is   │
  │                                    │  set from it each frame, not         │
  │                                    │  accumulated. No drift possible      │
  │                                    │  when music is playing.              │
  ├────────────────────────────────────┼──────────────────────────────────────┤
  │  Player dies mid-song              │  StopMusicStream(). SongState:       │
  │                                    │  playing = false, finished = true.   │
  │                                    │  Restart reloads from beat 0.        │
  ├────────────────────────────────────┼──────────────────────────────────────┤
  │  Song ends (song_time > duration)  │  All beats spawned. No more spawn.  │
  │                                    │  Wait for last obstacle to scroll    │
  │                                    │  past, then trigger results screen.  │
  ├────────────────────────────────────┼──────────────────────────────────────┤
  │  Game paused                       │  song_playback_system guards on      │
  │                                    │  GamePhase::Playing. song_time       │
  │                                    │  freezes. Music: PauseMusicStream(). │
  │                                    │  Resume: ResumeMusicStream().        │
  ├────────────────────────────────────┼──────────────────────────────────────┤
  │  Multiple obstacles same beat_time │  beat_scheduler spawns all of them   │
  │                                    │  in the same frame. This is valid —  │
  │                                    │  collision checks each independently.│
  └────────────────────────────────────┴──────────────────────────────────────┘
```

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 8 — MEMORY BUDGET
# ═══════════════════════════════════════════════════

```
  ┌────────────────────────────┬──────────┬───────────┬──────────────┐
  │  Data                      │ Per-elem │ Max count │ Total bytes  │
  ├────────────────────────────┼──────────┼───────────┼──────────────┤
  │  BeatMap.beats vector      │    8B    │   256     │    2,048     │
  │  SongState singleton       │   64B    │     1     │       64     │
  │  SongResults singleton     │   32B    │     1     │       32     │
  │  MusicContext singleton    │   ~40B   │     1     │       40     │
  │  BeatMap strings + fields  │  ~128B   │     1     │      128     │
  │  Live obstacles (entities) │  ~80B    │    ~10    │      800     │
  │  nlohmann::json (parse)    │  ~64KB   │     1     │   65,536     │
  │  raylib music stream       │  ~8KB    │     1     │    8,192     │
  ├────────────────────────────┼──────────┼───────────┼──────────────┤
  │  TOTAL ADDED               │          │           │   ~77 KB     │
  └────────────────────────────┴──────────┴───────────┴──────────────┘

  nlohmann::json temporary is freed after load_beat_map returns.
  Steady-state overhead: ~3 KB (singletons + beats vector).
  raylib music stream buffer is internal to raylib, managed automatically.
```

---

*Generated from: `content/beatmaps/2_drama_analysis.json` → `tools/level_designer.py` → `content/beatmaps/2_drama_beatmap.json`*
*Audio source: `content/audio/2_drama.flac` (33 MB, 241.13s, 119.64 BPM)*
