# SHAPESHIFTER — Beatmap & Audio Integration Spec
## Dev Spec · v2.0

> **Depends on:** `rhythm-spec.md` (component shapes, system signatures), `rhythm-design.md` (design rationale)
>
> **Inputs consumed:**
> - `content/beatmaps/<song_id>_beatmap.json` — authored obstacle timeline
> - `content/audio/<song_id>.flac` — song audio file
>
> **Scope:** Load a beatmap JSON + FLAC into the running game, replace random spawning with beat-driven spawning, and synchronise audio playback to `song_time`. This spec covers the data layouts, system transforms, build pipeline changes, and audio integration via raylib.

```
  ┌──────────────────────────────────────────────────────────────────┐
  │  CURRENT STATE (main branch — audio-backed runtime shipped)      │
  │                                                                  │
  │  ✅ raylib for rendering, input, windowing                      │
  │  ✅ nlohmann-json in vcpkg.json                                 │
  │  ✅ BeatMap, SongState, SongResults in rhythm.h                 │
  │  ✅ app/entities/beat_map.{h,cpp} — JSON → BeatMap with validation │
  │  ✅ song_playback_system — uses GetMusicTimePlayed() when audio │
  │     is loaded, with += dt fallback for silent/test mode         │
  │  ✅ beat_scheduler_system — spawns obstacles from BeatMap       │
  │  ✅ beat_scheduler_system is the runtime obstacle source        │
  │  ✅ shape_window_system — timing tier grading                   │
  │  ✅ Energy depletion owns terminal failure                      │
  │  ✅ InitAudioDevice / LoadMusicStream / playback code shipped   │
  │  ✅ CMake copies fonts, beatmaps, audio, UI, constants, shaders │
  │                                                                  │
  │  REMAINING INTEGRATION WORK                                     │
  │                                                                  │
  │    • future blocking obstacle kinds remain explicitly non-shipped scope │
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

> ⚠️ **Shipped implementation — required obstacles are `shape_gate` entries today.**
> Shipped beatmaps may also contain `onset_marker` entries; these preserve onset
> metadata and are skipped by the runtime scheduler. Archived/future `low_bar`
> and `high_bar` names are not runtime-supported authoring values.

```json
// shape_gate — currently the only required obstacle type in shipped beatmaps
{ "beat": 12, "kind": "shape_gate", "shape": "circle", "lane": 0 }

// onset_marker — non-blocking onset metadata, imported/exported by tools,
// parsed by runtime, and skipped by beat_scheduler_system.
{ "beat": 12, "kind": "onset_marker", "shape": "circle", "lane": 0 }
```

Archived/future design-space names such as `low_bar` and `high_bar` must not be emitted by current tools.

**Key field: `beat` is an INDEX, not a timestamp.** The game resolves `beat` → time via: `offset + beat × (60.0 / bpm)`.

## 1.2 Audio FLAC

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

## 2.1 Runtime Data — ALREADY IMPLEMENTED

Beatmap data is attached to the singleton `BeatMapTag` entity. Runtime state
singletons (`SongState`, `EnergyState`, `SongResults`) live in registry context.
All are cold data read a few times per frame, not iterated in bulk.

```cpp
// Defined in beat_map.h and re-exported by rhythm.h. The authored kind and
// shape are carried as *table membership* in BeatMap's fourteen
// per-(kind, shape, time-source) vectors (issues #1198 / #1202 / #1204 /
// #1533) — see rhythm-spec.md for the canonical structure. Spawned obstacle
// entities discover their archetype via `ShapeGateTag` / `SplitPathTag` /
// `OnsetMarkerTag` from `app/tags/tags.h`.
struct BeatEntry {
    int    beat_index = 0;       //  4B  (indexed bins only — *_beats)
    int8_t lane       = 1;       //  1B
    // No `shape`, `time_sec`, or `has_time_sec` columns — discriminator
    // is the vector identity.
};

struct BeatEntryTimed {
    int    beat_index = 0;       //  4B  (authored-onset bins — *_beats_timed)
    int8_t lane       = 1;       //  1B
    float  time_sec   = 0.0f;    //  4B  always-meaningful authored timestamp
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
    std::vector<float> beat_times;
    // 7 indexed + 7 timed vectors (see rhythm-spec.md / app/components/beat_map.h)
};

struct SongState {
    // …session-init, derived, per-frame mutable fields…
    // 7 indexed + 7 timed beat-schedule cursors (issue #1533).
};
```

### 2.1.1 MusicContext (singleton for audio state) — ✅ IMPLEMENTED

```cpp
struct MusicContext {
    Music  stream  = {};       // raylib Music handle (streaming)
    bool   loaded  = false;    // true if LoadMusicStream succeeded
    bool   started = false;    // true after first PlayMusicStream call
    bool   paused  = false;    // true while paused
    float  volume  = 0.8f;    // master music volume [0.0, 1.0]
};
```

Defined in `app/systems/audio_resources.h` (relocated from the never-shipped
`app/components/music.h` per #1351 — it ships beside its owning systems).
It owns the raylib `Music` handle and is
emplaced in `reg.ctx()` alongside `SongState`.

## 2.2 Existing Components — No Changes Needed

The current obstacle components already match the beatmap schema:

> ⚠️ **Shipped scope note:** Only `ShapeGate` is currently produced by `tools/level_designer.py`
> and emitted in shipped beatmaps. LowBar/HighBar were removed from the runtime obstacle enum.

```
  ShapeGateTag    → "shape_gate"       ✓  required shipped obstacle
  SplitPathTag    → "split_path"       ✓  runtime-supported, not generated today
  OnsetMarkerTag  → "onset_marker"     ✓  non-blocking shipped metadata

  `RequiredShape*Tag` (per-shape) → beatmap "shape" field  ✓
```

**No new per-entity components required for shape_gate.** The beat scheduler
creates the standard runtime obstacle archetypes via the kind-specific
`spawn_<kind>_rhythm()` helpers in `app/entities/obstacle_entity.h`
(`spawn_shape_gate_rhythm`, `spawn_split_path_rhythm`,
`spawn_onset_marker_rhythm`).

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 3 — SYSTEM TRANSFORMS
# ═══════════════════════════════════════════════════

## 3.1 Data Flow Diagram

```
  ┌──────────────────────────────────────────────────────────────────┐
  │  FRAME PIPELINE (on main branch — raylib + EnTT)                │
  │                                                                  │
  │  PER-FRAME (variable dt — game_loop.cpp):                        │
  │  ✅ compute_screen_transform   (refresh letterbox + scale)      │
  │  ✅ input_system               (raylib → InputState + events)   │
  │  ✅ ui_update_system                  (HUD buttons → events)    │
  │  ✅ test_player_system         (autoplay perception/planning)   │
  │       │                                                          │
  │       ▼  while (accumulator >= FIXED_DT) tick_fixed_systems(dt) │
  │       │                                                          │
  │  ✅ game_camera_system         (camera follow)                  │
  │  ✅ ui_camera_system           (HUD camera)                     │
  │  ✅ game_render_system         (world target)                   │
  │  ✅ ui_render_system           (UI target)                      │
  │  ✅ audio_system               (dispatcher → SFX playback)      │
  │  ✅ haptic_system              (HapticEvent dispatch)           │
  │                                                                  │
  │  FIXED TICK (tick_fixed_systems — fixed_tick_runner.cpp):        │
  │  ✅ game_state_system          (phase + semantic input drain)   │
  │  ✅ song_playback_system       (advances SongState.song_time)   │
  │  ✅ tick_playing_systems       (gated on GamePhasePlayingTag) │
  │  ✅ obstacle_despawn_system    (destroy off-camera obstacles)   │
  │  ✅ popup_feedback_system      (drain ScorePopupRequestQueue)   │
  │  ✅ popup_display_system       (ScorePopup timer → fade/cull)   │
  │  ✅ energy_system              (drain PendingEnergyEffectTag rows)│
  │  ✅ energy_bar_system          (HUD bar visual update)          │
  │  ✅ particle_system            (ParticleData timer → cull)      │
  │                                                                  │
  │  PLAYING TICK (tick_playing_systems — playing_systems_runner):   │
  │  ✅ beat_log_system            (per-beat diagnostic log)        │
  │  ✅ beat_scheduler_system      (BeatMap → spawn entities)       │
  │  ✅ shape_window_activation_system (early MorphIn tick)         │
  │  ✅ player_movement_system     (animate WorldPosition)         │
  │  ✅ scroll_system              (WorldPosition += MotionVel·dt) │
  │  ✅ motion_system              (additional motion components)   │
  │  ✅ collision_system           (player vs obstacles → ScoredTag)│
  │  ✅ shape_window_system        (runs AFTER collision — see #871)│
  │  ✅ miss_detection_system      (window expired → MissTag)       │
  │  ✅ scoring_system             (ScoredTag → ScoreState + popups)│
  └──────────────────────────────────────────────────────────────────┘

  ✅ = implemented and live in app/
```

## 3.2 app/entities/beat_map (loader) — ✅ ALREADY IMPLEMENTED

```
  app/entities/beat_map.h / .cpp — on main branch.
  Loads JSON → BeatMap, validates, inits SongState.
  Exposes load_beat_map() and load_and_validate_beat_map().
  Uses nlohmann-json. Called once at startup.
```

## 3.3 song_playback_system — ✅ IMPLEMENTED

```
  app/systems/song_playback_system.cpp — on main branch.
  Authoritative clock: GetMusicTimePlayed(music) when the song is
  loaded and started; falls back to song_time += dt for silent/test mode.
  UpdateMusicStream(music) is pumped every frame (even while paused) to
  prevent stream-buffer underruns.

  Music lifecycle is driven by GamePhase*Tag presence on reg.ctx():
    GamePhasePlayingTag       → PlayMusicStream / ResumeMusicStream
    GamePhasePausedTag        → PauseMusicStream
    GamePhaseGameOverTag    \
    GamePhaseSongCompleteTag /→ StopMusicStream + clear started/paused
  A restart_music handshake from setup_play_session is consumed on the
  next tick: it stops + replays the stream so each new session starts
  from t = 0 with a clean buffer.

  Beat advancement (Playing phase only). The former `int current_beat = -1`
  sentinel on SongState was migrated to the BeatCursor ctx-singleton row
  table (Fabian Principle 3 / issue #1545) — the row is absent until the
  first beat is crossed; while it exists, `last_crossed` is the highest
  beat index that has been crossed and is always meaningful.

  When a BeatMap is loaded, beat crossings advance through
  `map->beat_times[]` (authored). Otherwise the BPM-derived fallback
  is used (song_time − offset) / beat_period. Both paths apply the
  user audio offset from `settings::audio_offset_seconds(*settings)` so
  the crossing time matches the apparent audio position.

  Song end: when song_time ≥ duration_sec, sets finished + clears playing
  and stops the music stream so the next session starts cleanly.
```

## 3.4 beat_scheduler_system — ✅ ALREADY IMPLEMENTED

```
  Spawns obstacle entities from BeatMap.beats[] when song_time
  crosses spawn_time thresholds. It calls the kind-specific
  spawn_<kind>_rhythm() helpers (spawn_shape_gate_rhythm,
  spawn_split_path_rhythm, spawn_onset_marker_rhythm) in
  app/entities/obstacle_entity.h, so collision, scoring, render,
  and despawn all work unchanged.
```

## 3.5 Runtime obstacle source — ✅ BEAT-SCHEDULED

```
  beat_scheduler_system is the runtime obstacle source for authored levels.
  If a beatmap fails to load, no authored obstacles are spawned for that run.
```

## 3.6 audio_system — ✅ IMPLEMENTED (raylib)

```
  Input:  PlaySfxEvent events queued on entt::dispatcher
          SFXBank resident raylib Sound handles
  Output: raylib PlaySound calls for queued SFX
```

```
  ┌──────────────────────────────────────────────────────────────┐
  │  Audio Architecture (raylib)                                 │
  │                                                              │
  │  Song:  LoadMusicStream(song_path) → PlayMusicStream(music)  │
  │         Loaded per play session. raylib streams from disk.   │
  │         Playback sync: GetMusicTimePlayed(music)             │
  │         UpdateMusicStream(music) runs every frame in         │
  │         song_playback_system.                                │
  │                                                              │
  │  SFX:   sfx_bank_init loads Sound handles at startup.        │
  │         audio_system drains PlaySfxEvent and calls PlaySound.│
  │         Channel allocation automatic.                        │
  │                                                              │
  │  Init:  InitAudioDevice()                                    │
  │         Called once in game_loop_init() before the loop.     │
  │                                                              │
  │  Shutdown: UnloadMusicStream + UnloadSound × N               │
  │            CloseAudioDevice()                                │
  └──────────────────────────────────────────────────────────────┘
```

`MusicContext` owns the raylib `Music` handle for the active song. `audio_system`
is intentionally SFX-only; music lifecycle, pause/resume, stream pumping, and
time sync live in `song_playback_system`.

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

## 4.2 CMakeLists.txt — ✅ CONTENT COPY RULES IMPLEMENTED

`CMakeLists.txt` links raylib and nlohmann-json and copies runtime assets next
to the executable. Native builds copy fonts, beatmaps, audio, UI configs,
shared constants, and shaders. Emscripten builds preload `content/`.

## 4.3 Content Asset Copying — ✅ IMPLEMENTED

Current native asset-copy rules:

```cmake
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
  ✅ app/entities/beat_map.h / .cpp       — JSON → BeatMap with validation
  ✅ app/systems/song_playback_system.cpp — song_time advancement
  ✅ app/systems/beat_scheduler_system.cpp — obstacle spawning from BeatMap
  ✅ app/systems/shape_window_system.cpp  — timing window morphing
  ✅ app/systems/energy_system.cpp        — miss tracking and terminal failure
  ✅ app/systems/audio_system.cpp         — dispatcher-driven SFX playback
  ✅ app/systems/audio_resources.h        — MusicContext (raylib Music handle + state flags)
  ✅ app/game_loop.cpp                    — InitAudioDevice + audio lifecycle
  ✅ app/systems/play_session.cpp         — LoadMusicStream per play session
```

`audio_system.cpp` is already in `app/systems/` so it's auto-included in
`shapeshifter_lib` via the `file(GLOB)` rule. No CMake changes for source files.

---
---
---

# ═══════════════════════════════════════════════════
# SECTION 5 — GAME LOOP / PLAY SESSION INTEGRATION
# ═══════════════════════════════════════════════════

## 5.1 Singleton Init — ✅ DONE

Beat map and music loading are handled by `setup_play_session()`. `game_loop_init()`
creates the beat-map entity and emplaces `SongState`, `EnergyState`,
`SongResults`, and `MusicContext`.

```cpp
auto* music = reg.ctx().find<MusicContext>();
if (music) music->release();
if (music && !beatmap.song_path.empty()) {
    Music stream = LoadMusicStream(path);
    if (music_stream_is_playable(stream)) {
        music->stream = stream;
        music->loaded = true;
        SetMusicVolume(music->stream, music->volume);
    }
}
```

## 5.2 Audio Init — ✅ DONE

```cpp
// After InitWindow(), before main loop:
InitAudioDevice();

// Verify:
if (!IsAudioDeviceReady()) {
    TraceLog(LOG_WARNING, "Audio device not available — running silent");
}
```

## 5.3 Music Playback Start — ✅ DONE

Music starts or resumes from `song_playback_system` when the active phase is
`Playing` and `SongState.playing` is true:

```cpp
if (music_loaded && reg.ctx().contains<GamePhasePlayingTag>() && song && song->playing) {
    if (!music->started) {
        PlayMusicStream(music->stream);
        music->started = true;
    } else if (music->paused) {
        ResumeMusicStream(music->stream);
    }
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

## 5.5 Shutdown — ✅ DONE

```cpp
if (auto* music = reg.ctx().find<MusicContext>()) music->release();
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
  • BeatMap, BeatEntry, SongState, SongResults, EnergyState in rhythm.h/song_state.h
  • TimingGrade (precision-only), ShapeWindow*Tag (per-phase tags in
    `app/tags/tags.h`), BeatInfo all defined

  STEP 3 — Beat Map Loader                         ✅ DONE
  ─────────────────────────────
  • app/entities/beat_map.h/.cpp: JSON → BeatMap with validation
  • Parser supports ShapeGate, SplitPath, and onset_marker only.
  • init_song_state() computes derived fields from BPM

  STEP 4 — Song Playback + Beat Scheduler          ✅ DONE
  ──────────────────────────────
  • song_playback_system: advances song_time, detects song end
  • beat_scheduler_system: spawns obstacles from BeatMap.beats[]
  • no separate random obstacle spawner is part of the current runtime path

  STEP 5 — Content Asset Copying                    ✅ DONE
  ────────────────────────────────
  • CMake copies content/beatmaps/*.json and content/audio/*.flac next to the executable
  • CMake also copies fonts, UI configs, constants, and shader assets

  STEP 6 — Audio Playback (raylib)                  ✅ DONE
  ──────────────────────────────────
  • MusicContext owns the raylib Music handle + state flags
  • game_loop_init() calls InitAudioDevice() before play
  • setup_play_session() loads music for the selected beatmap
  • song_playback_system starts, pauses, resumes, stops, updates, and syncs music
  • game_loop_shutdown() releases music and closes the audio device

  STEP 7 — Validation                               ✅ DONE
  ────────────────────
  • Loader validates beatmap schema and timing/content invariants
  • Shipped beatmap tests cover difficulty ramps, shape gaps, max gaps, and readability
  • Audio offset calibration tests cover timing alignment paths
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
  │                                    │  BeatMap remains empty for the run.  │
  ├────────────────────────────────────┼──────────────────────────────────────┤
  │  FLAC file missing                 │  music_stream_is_playable() fails.   │
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
  │                                    │  GamePhasePlayingTag. song_time      │
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
  │  SongState singleton       │  160B    │     1     │      160     │
  │  SongResults singleton     │   28B    │     1     │       28     │
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
