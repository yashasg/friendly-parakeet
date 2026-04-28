
# SHAPESHIFTER: Audio & Content Data Pipeline — Implementation Specification

## Data Flow Diagram

```
DISK                      PARSE                         SINGLETONS (reg.ctx())                    SYSTEMS (per frame)
─────────────────────     ───────────────               ──────────────────────                     ───────────────────

content/beatmaps/         parse_beat_map()              ┌──────────┐
  2_drama_beatmap.json ────────────────────────────────▶│ BeatMap  │─────────────────────────────▶ beat_scheduler_system
                          reads difficulties[medium]     │ .beats[] │                               reads beats[next_spawn_idx]
                          derives song_path              │ .song_path                               creates obstacle entities
                                                         └────┬─────┘
                                                              │
                                                         init_song_state()
                                                              │
                                                              ▼
                                                         ┌──────────┐
                                                         │SongState │◀────────────────────────────  song_playback_system
                                                         │.song_time│  writes song_time, current_beat
                                                         │.playing  │  reads MusicContext for clock
                                                         └──────────┘

content/audio/            LoadMusicStream()              ┌──────────────┐
  2_drama.flac ───────────────────────────────────────▶  │ MusicContext  │◀───────────────────────  song_playback_system
                                                         │ .stream      │  UpdateMusicStream()
                                                         │ .loaded      │  GetMusicTimePlayed()
                                                         │ .started     │  PlayMusicStream()
                                                         └──────────────┘

                                                         ┌──────────────┐
                                                         │ AudioQueue   │◀───────────────────────  collision/hp/game_state
                                                         │ .queue[16]   │  audio_push(SFX)
                                                         │ .count       │
                                                         └──────┬───────┘
                                                                │
                                                                ▼
                                                           audio_system ─────────────────────────  drains queue, plays SFX
                                                           (future: PlaySound per entry)
```

### Clock Authority Chain

```
raylib audio decoder
        │
        ▼
GetMusicTimePlayed(stream)   ← authoritative source (sample-accurate)
        │
        ▼
song_time = music_time       ← SongState.song_time mirrors it
        │
        ├──▶ beat_scheduler_system: compares song_time against spawn_times
        │
        └──▶ song_playback_system: computes current_beat from song_time

FALLBACK (no MusicContext): song_time += dt  (silent / test mode)
```

---

## Change 1: CMake Content Copy Rules

### Data
No struct changes. Adds file copy targets for:
- `content/beatmaps/*.json` — beatmap chart files (~30KB each)
- `content/audio/*.flac` — FLAC audio files (~30MB each)

### File
**`CMakeLists.txt`** — insert after line 186 (the existing font copy `endif()`), before the `# ── Tests` comment on line 188.

### Code

```cmake
# Copy beatmap JSON files next to the executable
file(GLOB BEATMAP_FILES ${CMAKE_SOURCE_DIR}/content/beatmaps/*.json)
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

# Copy audio files next to the executable
file(GLOB AUDIO_FILES ${CMAKE_SOURCE_DIR}/content/audio/*.flac)
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

### Emscripten path
The existing Emscripten linker flags on line 170 preload only `assets@/assets`. Add a second `--preload-file` for content. Modify the `target_link_options` block:

```cmake
    target_link_options(shapeshifter PRIVATE
        -sUSE_GLFW=3
        -sALLOW_MEMORY_GROWTH=1
        --shell-file ${_shell_html}
        --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets
        --preload-file ${CMAKE_SOURCE_DIR}/content@/content
    )
```

### Dependencies
None — first change to apply.

### Verification
```bash
cmake --build build && ls build/content/beatmaps/ build/content/audio/
# Expected: 2_drama_beatmap.json and 2_drama.flac present
```

---

## Change 2: Loader — Handle Nested Difficulties

### Data
**No new structs.** `BeatMap` already has a `difficulty` field (rhythm.h line 56) and a `song_path` field (line 51). We modify `parse_beat_map()` behavior only.

### File
**`app/beat_map_loader.h`** — signature change:
```cpp
// Add difficulty parameter with default
bool parse_beat_map(const std::string& json_str, BeatMap& out,
                    std::vector<BeatMapError>& errors,
                    const std::string& difficulty = "medium");

bool load_beat_map(const std::string& json_path, BeatMap& out,
                   std::vector<BeatMapError>& errors,
                   const std::string& difficulty = "medium");
```

### File
**`app/beat_map_loader.cpp`** — replace `parse_beat_map()` body (lines 26–80) and update `load_beat_map()` signature (line 82).

### Code — full replacement of `parse_beat_map`

```cpp
bool parse_beat_map(const std::string& json_str, BeatMap& out,
                    std::vector<BeatMapError>& errors,
                    const std::string& difficulty) {
    json j;
    try {
        j = json::parse(json_str);
    } catch (const json::parse_error& e) {
        errors.push_back({-1, std::string("JSON parse error: ") + e.what()});
        return false;
    }

    // ── Metadata ────────────────────────────────────────────
    out.song_id    = j.value("song_id", "");
    out.title      = j.value("title", "");
    out.bpm        = j.value("bpm", 120.0f);
    out.offset     = j.value("offset", 0.0f);
    out.lead_beats = j.value("lead_beats", 4);
    out.duration   = j.value("duration_sec", 180.0f);
    out.difficulty  = difficulty;

    // ── song_path: explicit field, or derive from song_id ───
    if (j.contains("song_path") && j["song_path"].is_string()) {
        out.song_path = j["song_path"].get<std::string>();
    } else if (!out.song_id.empty()) {
        out.song_path = "content/audio/" + out.song_id + ".flac";
    }

    // ── Beats: nested difficulties (preferred) or flat array ─
    const json* beats_array = nullptr;

    if (j.contains("difficulties") && j["difficulties"].is_object()) {
        const auto& diffs = j["difficulties"];
        if (diffs.contains(difficulty) && diffs[difficulty].contains("beats")
            && diffs[difficulty]["beats"].is_array()) {
            beats_array = &diffs[difficulty]["beats"];
        } else {
            // Requested difficulty not found — try fallback order
            const char* fallbacks[] = {"medium", "easy", "hard"};
            for (const char* fb : fallbacks) {
                if (diffs.contains(fb) && diffs[fb].contains("beats")
                    && diffs[fb]["beats"].is_array()) {
                    beats_array = &diffs[fb]["beats"];
                    out.difficulty = fb;
                    errors.push_back({-1, std::string("Difficulty '") + difficulty
                        + "' not found, falling back to '" + fb + "'"});
                    break;
                }
            }
        }
    }

    // Backward compat: flat top-level beats array
    if (!beats_array && j.contains("beats") && j["beats"].is_array()) {
        beats_array = &j["beats"];
    }

    if (!beats_array) {
        errors.push_back({-1, "No beats found in beatmap"});
        return false;
    }

    // ── Parse individual beat entries ────────────────────────
    for (const auto& b : *beats_array) {
        BeatEntry entry;
        entry.beat_index = b.value("beat", 0);

        std::string kind_str = b.value("kind", "shape_gate");
        entry.kind = parse_kind(kind_str);

        if (b.contains("shape")) {
            entry.shape = parse_shape(b["shape"].get<std::string>());
        }

        entry.lane = static_cast<int8_t>(b.value("lane", 1));

        if (b.contains("blocked") && b["blocked"].is_array()) {
            entry.blocked_mask = 0;
            for (const auto& lane_idx : b["blocked"]) {
                int l = lane_idx.get<int>();
                if (l >= 0 && l < 3) {
                    entry.blocked_mask |= static_cast<uint8_t>(1 << l);
                }
            }
        }

        out.beats.push_back(entry);
    }

    // Sort beats by beat_index (chart may not be pre-sorted)
    std::sort(out.beats.begin(), out.beats.end(),
              [](const BeatEntry& a, const BeatEntry& b) {
                  return a.beat_index < b.beat_index;
              });

    return true;
}
```

### Code — updated `load_beat_map` signature

```cpp
bool load_beat_map(const std::string& json_path, BeatMap& out,
                   std::vector<BeatMapError>& errors,
                   const std::string& difficulty) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        errors.push_back({-1, "Could not open file: " + json_path});
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    return parse_beat_map(content, out, errors, difficulty);
}
```

### Dependencies
None. Can be done in parallel with Change 1.

### Verification
Existing unit tests (if any for beat_map_loader) should still pass with flat-array JSON. To verify nested format:
```cpp
// In a test:
BeatMap map;
std::vector<BeatMapError> errs;
bool ok = load_beat_map("content/beatmaps/2_drama_beatmap.json", map, errs, "easy");
assert(ok);
assert(map.beats.size() == 80);  // easy difficulty has 80 beats
assert(map.song_path == "content/audio/2_drama.flac");
assert(map.difficulty == "easy");
```

---

## Change 3: MusicContext Singleton

### Data

Add to **`app/components/rhythm.h`**, after the `SongResults` struct (after line 103, before the helper functions):

```cpp
// ── Music Context (singleton, holds raylib Music handle) ────
// Cold data: read/written 1-2x per frame by song_playback_system.
// raylib's Music is an opaque struct (~32 bytes, contains internal
// pointer to AudioStream + metadata). We wrap it to track load state.
struct MusicContext {
    Music stream{};       // raylib Music handle (zero-initialized)
    bool  loaded  = false; // LoadMusicStream succeeded
    bool  started = false; // PlayMusicStream has been called
    float volume  = 0.8f;  // master volume [0.0, 1.0]
};
```

**Byte budget:** raylib's `Music` struct is defined in raylib.h as:
```c
typedef struct Music {
    AudioStream stream;  // ~24 bytes
    unsigned int frameCount;
    bool looping;
    int ctxType;
    void *ctxData;
} Music;  // ~48 bytes total on 64-bit
```
Plus our 3 fields: `bool loaded` (1), `bool started` (1), `float volume` (4) + padding = ~56 bytes total. This is cold data — accessed 1–2× per frame by `song_playback_system` only. Not in any hot loop. No cache concern.

### Header dependency note
`rhythm.h` currently does **not** include `<raylib.h>`. Since `Music` is a raylib type, the `MusicContext` struct introduces a raylib dependency in this header. This is acceptable because:
1. `rhythm.h` is only included by system `.cpp` files that already transitively depend on raylib through the executable link
2. `shapeshifter_lib` does not link raylib — but `MusicContext` is only **used** (emplaced/accessed) in files that link against raylib (main.cpp, song_playback_system.cpp via the exe, audio_system.cpp)

**However**, to avoid a build break in `shapeshifter_lib` (which compiles song_playback_system.cpp and doesn't link raylib), we need a **conditional approach**. Two options:

**Option A (recommended): Put MusicContext in a separate header**

Create **`app/components/music.h`**:

```cpp
#pragma once

#include <raylib.h>

// ── Music Context (singleton, holds raylib Music handle) ────
// Cold data: read/written 1-2x per frame by song_playback_system.
struct MusicContext {
    Music stream{};        // raylib Music handle
    bool  loaded  = false; // LoadMusicStream succeeded
    bool  started = false; // PlayMusicStream has been called
    float volume  = 0.8f;  // master volume [0.0, 1.0]
};
```

**Why separate header:** `shapeshifter_lib` compiles system `.cpp` files without linking raylib. The `Music` type requires `<raylib.h>`. Files that use `MusicContext` (main.cpp, audio_system.cpp — both linked to the exe) include `music.h` explicitly. Files in the lib (beat_scheduler_system.cpp, etc.) never touch `MusicContext` and never include this header. song_playback_system.cpp needs it — see the lib linkage concern below.

**Critical linkage concern:** `song_playback_system.cpp` is in `shapeshifter_lib` (it matches `app/systems/*.cpp` and is NOT excluded). It will need to include `<raylib.h>` for `GetMusicTimePlayed()`, `UpdateMusicStream()`, etc. This means **shapeshifter_lib must link raylib** OR song_playback_system.cpp must be excluded from the lib and linked directly to the exe (like render_system.cpp and input_system.cpp).

**Decision: Exclude song_playback_system.cpp from the lib.** It calls raylib audio functions, making it a raylib-dependent system like render_system and input_system.

Add to CMakeLists.txt line 78 (after the input_system exclude):
```cmake
list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "song_playback_system\\.cpp$")
```

And add it to the exe source list (CMakeLists.txt line 101):
```cmake
add_executable(shapeshifter
    app/main.cpp
    app/systems/render_system.cpp
    app/systems/input_system.cpp
    app/systems/song_playback_system.cpp
    app/text_renderer.cpp
    app/file_logger.cpp
)
```

Similarly, `audio_system.cpp` will call raylib audio functions. But the user stated "audio_system.cpp is NOT excluded — it should go in the lib." We need to reconcile this. If `audio_system.cpp` calls `PlaySound()` (raylib), it can't be in the lib without linking raylib to the lib.

**Resolution: audio_system.cpp must also be excluded from lib and linked to the exe**, same pattern as render_system.cpp. The user's statement that it "should go in the lib" was based on the stub that doesn't call raylib. Once it calls raylib functions, it must be linked to the exe. For now, it's a stub that only clears AudioQueue — no raylib calls. **Keep it in the lib for now. When SFX playback is added later, move it to the exe link list.**

Actually — let me re-examine. The current audio_system stub is inside render_system.cpp. We're creating a new file. If the new file only clears AudioQueue (no raylib calls), it can stay in the lib. That's the current plan per Change 6. Good.

### Dependencies
Change 1 (content copy) should be done first so file paths resolve. But this header can be created at any time.

### Verification
```cpp
static_assert(std::is_aggregate_v<MusicContext>);
// Compiles: no constructors, no virtuals, plain struct.
```

---

## Change 4: main.cpp — Audio Init + Beatmap Loading

### Data
No new structs. Uses `BeatMap`, `SongState`, `MusicContext` singletons.

### File
**`app/main.cpp`** — modifications at 4 points:

### 4a. Add includes (after line 14)

```cpp
#include "components/music.h"    // MusicContext singleton
#include "beat_map_loader.h"     // load_beat_map, init_song_state
```

### 4b. Init audio device (after line 83, after `SetTargetFPS(60)`)

```cpp
    InitAudioDevice();
    TraceLog(LOG_INFO, "Audio device initialized: %d Hz", 44100);
```

### 4c. Replace empty BeatMap + hardcoded SongState (replace lines 133–139)

Current code (lines 133–139):
```cpp
    // Rhythm singletons (active even without a beat map loaded)
    auto& song_state = reg.ctx().emplace<SongState>();
    song_state.bpm = 120.0f;
    song_state.playing = true;
    song_state_compute_derived(song_state);
    reg.ctx().emplace<BeatMap>();
    reg.ctx().emplace<HPState>();
    reg.ctx().emplace<SongResults>();
```

Replace with:
```cpp
    // ── Rhythm singletons ────────────────────────────────────
    reg.ctx().emplace<HPState>();
    reg.ctx().emplace<SongResults>();

    // Load beatmap from disk
    {
        auto& beatmap = reg.ctx().emplace<BeatMap>();
        std::vector<BeatMapError> load_errors;

        // Try path relative to executable first, then CWD
        std::string exe_beatmap = std::string(GetApplicationDirectory())
                                + "content/beatmaps/2_drama_beatmap.json";
        const char* beatmap_paths[] = {
            exe_beatmap.c_str(),
            "content/beatmaps/2_drama_beatmap.json",
        };

        bool loaded = false;
        for (const char* path : beatmap_paths) {
            load_errors.clear();
            if (load_beat_map(path, beatmap, load_errors, "medium")) {
                TraceLog(LOG_INFO, "Loaded beatmap: %s (%zu beats, difficulty=%s)",
                         path, beatmap.beats.size(), beatmap.difficulty.c_str());
                loaded = true;
                break;
            }
        }

        if (!loaded) {
            TraceLog(LOG_WARNING, "No beatmap loaded — running in freeplay mode");
            for (const auto& err : load_errors) {
                TraceLog(LOG_WARNING, "  beatmap error: %s", err.message.c_str());
            }
        }

        // Validate if loaded
        if (loaded) {
            std::vector<BeatMapError> val_errors;
            if (!validate_beat_map(beatmap, val_errors)) {
                TraceLog(LOG_WARNING, "Beatmap validation warnings:");
                for (const auto& err : val_errors) {
                    TraceLog(LOG_WARNING, "  beat %d: %s", err.beat_index, err.message.c_str());
                }
            }
        }

        // Initialize SongState from beatmap
        auto& song = reg.ctx().emplace<SongState>();
        if (!beatmap.beats.empty()) {
            init_song_state(song, beatmap);
        } else {
            song.bpm = 120.0f;
            song_state_compute_derived(song);
        }
        // Don't set song.playing = true here — enter_playing() handles that
    }

    // ── Load music stream ────────────────────────────────────
    {
        auto& music = reg.ctx().emplace<MusicContext>();
        auto* beatmap = reg.ctx().find<BeatMap>();

        if (beatmap && !beatmap->song_path.empty()) {
            // Try relative to exe first, then CWD
            std::string exe_audio = std::string(GetApplicationDirectory())
                                  + beatmap->song_path;
            const char* audio_paths[] = {
                exe_audio.c_str(),
                beatmap->song_path.c_str(),
            };

            for (const char* path : audio_paths) {
                Music stream = LoadMusicStream(path);
                if (stream.frameCount > 0) {
                    music.stream = stream;
                    music.loaded = true;
                    music.started = false;
                    SetMusicVolume(music.stream, music.volume);
                    TraceLog(LOG_INFO, "Loaded music: %s (%u frames)",
                             path, stream.frameCount);
                    break;
                }
            }

            if (!music.loaded) {
                TraceLog(LOG_WARNING, "Could not load music: %s", beatmap->song_path.c_str());
            }
        }
    }
```

### 4d. Shutdown — before `CloseWindow()` (modify lines 223–227)

Current code:
```cpp
    // ── SHUTDOWN ─────────────────────────────────────────────
    UnloadRenderTexture(target);
    text_shutdown(reg.ctx().get<TextContext>());
    CloseWindow();
    return 0;
```

Replace with:
```cpp
    // ── SHUTDOWN ─────────────────────────────────────────────
    {
        auto* music = reg.ctx().find<MusicContext>();
        if (music && music->loaded) {
            StopMusicStream(music->stream);
            UnloadMusicStream(music->stream);
            music->loaded = false;
            TraceLog(LOG_INFO, "Music stream unloaded");
        }
    }
    CloseAudioDevice();
    UnloadRenderTexture(target);
    text_shutdown(reg.ctx().get<TextContext>());
    CloseWindow();
    return 0;
```

**Order matters:** `UnloadMusicStream` before `CloseAudioDevice` before `CloseWindow`. raylib requires audio resources be freed before the audio device is closed.

### 4e. Emscripten path

The Emscripten `update_draw_frame()` function (lines 37–74) does not handle init — init happens in `main()` before `emscripten_set_main_loop()`. So the init code above works for both paths. The content files are available in the virtual filesystem via `--preload-file` (Change 1).

For Emscripten shutdown: `emscripten_set_main_loop` with arg `1` means it never returns to the code after it. Shutdown code after line 153 will never execute in Emscripten builds. This is acceptable — the browser manages resource cleanup. If explicit cleanup is needed, register an `atexit` handler, but for a game this is not required.

### Dependencies
- Change 1 (content copy) — so files exist at runtime
- Change 2 (loader nested format) — so `load_beat_map()` accepts difficulty parameter
- Change 3 (MusicContext) — so the struct exists

### Verification
```
# Build and run. Check log output:
# Expected:
#   Loaded beatmap: .../content/beatmaps/2_drama_beatmap.json (125 beats, difficulty=medium)
#   Loaded music: .../content/audio/2_drama.flac
#   Audio device initialized: 44100 Hz
```

---

## Change 5: song_playback_system — Authoritative Audio Clock

### Data
Reads: `SongState`, `MusicContext` (optional — existential check), `GameState`.
Writes: `SongState.song_time`, `SongState.current_beat`, `SongState.playing`, `SongState.finished`, `MusicContext.started`.

### File
**`app/systems/song_playback_system.cpp`** — full replacement.

**Also: CMakeLists.txt** — exclude from lib, add to exe (see Change 3 discussion).

### CMakeLists.txt modifications

Line 78 — add exclusion (after existing input_system exclude):
```cmake
list(FILTER SYSTEM_SOURCES EXCLUDE REGEX "song_playback_system\\.cpp$")
```

Line 101 — add to exe sources:
```cmake
add_executable(shapeshifter
    app/main.cpp
    app/systems/render_system.cpp
    app/systems/input_system.cpp
    app/systems/song_playback_system.cpp
    app/text_renderer.cpp
    app/file_logger.cpp
)
```

### Code — full file replacement

```cpp
#include "all_systems.h"
#include "../components/game_state.h"
#include "../components/rhythm.h"
#include "../components/music.h"
#include <raylib.h>

void song_playback_system(entt::registry& reg, float dt) {
    const auto& gs = reg.ctx().get<GameState>();

    auto* song  = reg.ctx().find<SongState>();
    auto* music = reg.ctx().find<MusicContext>();

    // ── Music stream update (MUST happen every frame, even when paused) ──
    // raylib streams audio in chunks — if UpdateMusicStream isn't called
    // frequently enough, the stream buffer underruns and playback stutters.
    if (music && music->loaded && music->started) {
        UpdateMusicStream(music->stream);
    }

    // ── Music lifecycle: start / pause / resume ─────────────
    if (music && music->loaded) {
        if (gs.phase == GamePhase::Playing && song && song->playing) {
            if (!music->started) {
                // First entry into Playing with music loaded
                PlayMusicStream(music->stream);
                music->started = true;
            } else if (gs.previous_phase == GamePhase::Paused) {
                // Resuming from pause
                ResumeMusicStream(music->stream);
            }
        } else if (gs.phase == GamePhase::Paused && music->started) {
            PauseMusicStream(music->stream);
        } else if (gs.phase == GamePhase::GameOver && music->started) {
            StopMusicStream(music->stream);
            music->started = false;
        }
    }

    // ── Song time advancement (only during Playing phase) ───
    if (gs.phase != GamePhase::Playing) return;
    if (!song || !song->playing || song->finished) return;

    // Authoritative clock from audio stream, fallback to dt accumulation
    if (music && music->loaded && music->started) {
        song->song_time = GetMusicTimePlayed(music->stream);
    } else {
        // Silent mode: no music loaded, accumulate dt
        song->song_time += dt;
    }

    // ── Current beat computation ────────────────────────────
    if (song->beat_period > 0.0f && song->song_time >= song->offset) {
        int beat = static_cast<int>((song->song_time - song->offset) / song->beat_period);
        if (beat > song->current_beat) {
            song->current_beat = beat;
        }
    }

    // ── Song end detection ──────────────────────────────────
    if (song->song_time >= song->duration_sec) {
        song->finished = true;
        song->playing  = false;
        if (music && music->loaded && music->started) {
            StopMusicStream(music->stream);
            music->started = false;
        }
    }
}
```

### Music lifecycle state machine

```
                  enter_playing()           Pause
  [not started] ──────────────────▶ [Playing] ──────────▶ [Paused]
                                     │    ▲                  │
                                     │    └──────────────────┘
                                     │        Resume
                                     │
                                     │  song end / game over
                                     ▼
                                   [Stopped]
                                   started=false

  PlayMusicStream():   not started → Playing
  PauseMusicStream():  Playing → Paused
  ResumeMusicStream(): Paused → Playing
  StopMusicStream():   any → Stopped (resets to beginning)
```

### enter_playing() interaction (game_state_system.cpp)

The existing `enter_playing()` on line 51-60 already handles the reset:
```cpp
auto* song = reg.ctx().find<SongState>();
auto* beatmap = reg.ctx().find<BeatMap>();
bool has_chart = beatmap && !beatmap->beats.empty();
if (song) {
    song->song_time      = 0.0f;
    song->current_beat   = -1;
    song->playing        = has_chart;
    song->finished       = false;
    song->next_spawn_idx = 0;
}
```

**Add music reset** inside `enter_playing()` (after line 60, before the HP reset on line 61):
```cpp
    auto* music = reg.ctx().find<MusicContext>();
    if (music && music->loaded) {
        StopMusicStream(music->stream);  // resets playhead to 0
        music->started = false;          // will be re-started by song_playback_system
    }
```

This requires adding includes to game_state_system.cpp:
```cpp
#include "../components/music.h"
#include <raylib.h>
```

**However**, game_state_system.cpp is in `shapeshifter_lib` and cannot call raylib. Two options:

**Option A: Exclude game_state_system.cpp from lib** — excessive, it's mostly non-raylib logic.

**Option B (recommended): Don't call StopMusicStream in enter_playing(). Instead, let song_playback_system detect the restart.**

song_playback_system already runs after game_state_system in the pipeline. When enter_playing() resets `song->playing = has_chart` and `song->song_time = 0`, song_playback_system sees `music->started == true` but `song_time == 0`. We handle this by checking if music needs a restart:

In song_playback_system.cpp, in the "start" block, replace:
```cpp
if (!music->started) {
```
with:
```cpp
if (!music->started || GetMusicTimePlayed(music->stream) > 0.5f) {
```

**Actually, cleaner approach:** Reset `music->started = false` in `enter_playing()` without calling any raylib function. song_playback_system will see `started == false` next frame, call `StopMusicStream` + `PlayMusicStream`:

In `enter_playing()` (game_state_system.cpp), after the song reset block:
```cpp
    // Signal music restart — song_playback_system will handle the raylib calls
    auto* music = reg.ctx().find<MusicContext>();
    if (music) {
        music->started = false;
    }
```

**This requires only `#include "../components/music.h"` — no raylib.h needed.** The `MusicContext` struct is a plain struct with no raylib function calls. The `music.h` header includes `<raylib.h>` for the `Music` type, though.

**Problem:** including `music.h` in a lib-compiled file still pulls in `<raylib.h>`, which will fail to find the header if the lib doesn't have raylib in its include path.

**Final resolution: Use a flag in SongState instead.**

Add a single bool to SongState (rhythm.h line 82, after `finished`):
```cpp
    bool   restart_music = false; // set by enter_playing(), consumed by song_playback
```

In enter_playing() (game_state_system.cpp), after the song reset:
```cpp
    if (song) {
        // ... existing resets ...
        song->restart_music = true;
    }
```

In song_playback_system.cpp, at the start of the Playing phase block:
```cpp
    // Handle music restart request (from enter_playing)
    if (song->restart_music) {
        song->restart_music = false;
        if (music && music->loaded) {
            StopMusicStream(music->stream);
            PlayMusicStream(music->stream);
            music->started = true;
        }
    }
```

**This is the cleanest separation.** No raylib dependency leaks into the lib. The flag is 1 byte in a cold singleton — zero cache impact.

### SongState modification (rhythm.h)

Add after line 81 (`bool finished = false;`):
```cpp
    bool   restart_music = false;  // signal: enter_playing() → song_playback_system
```

### Dependencies
- Change 3 (MusicContext struct + music.h)
- Change 4 (music loaded in main.cpp)

### Verification
1. Run game, tap to start → music plays, obstacles sync to beat
2. Die → music stops
3. Tap to restart → music restarts from beginning
4. Pause → music pauses. Resume → music resumes
5. Let song play for 3+ minutes → check that obstacles still align with audio beats (no drift)
6. Build without content files → falls back to silent += dt mode, no crash

---

## Change 6: audio_system.cpp — Create the File

### Data
Reads/writes: `AudioQueue` singleton.
Future: will read SFX Sound handles. For now, stub.

### File
**`app/systems/audio_system.cpp`** — NEW FILE

**Also: Remove the stub from `app/systems/render_system.cpp`** — delete lines 362–368.

### Code — new file

```cpp
#include "all_systems.h"
#include "../audio/audio_types.h"

void audio_system(entt::registry& reg) {
    auto* audio = reg.ctx().find<AudioQueue>();
    if (!audio) return;

    // Future: iterate audio->queue[0..count-1], call PlaySound() for each SFX.
    // SFX loading and playback will be added when sound assets are ready.
    // For now: drain the queue so it doesn't accumulate stale events.

    audio_clear(*audio);
}
```

### Code — remove stub from render_system.cpp

Delete lines 362–368 of `app/systems/render_system.cpp`:
```cpp
// Audio system stub — SFX playback requires sound loading
void audio_system(entt::registry& reg) {
    auto& audio = reg.ctx().get<AudioQueue>();
    // In a full implementation, iterate audio.queue[0..count-1]
    // and call PlaySound for each SFX
    audio_clear(audio);
}
```

### Linkage
The new `audio_system.cpp` matches `app/systems/*.cpp` glob and is NOT excluded → it gets compiled into `shapeshifter_lib`. It only includes `audio.h` (no raylib dependency). This is correct. When SFX playback is added (calling `PlaySound`), it must be excluded from the lib and added to the exe, same as render_system.

### Dependencies
None — can be done at any time. Just ensure the render_system.cpp stub is removed in the same commit to avoid duplicate symbol errors.

### Verification
```bash
cmake --build build 2>&1 | grep -i "error\|duplicate"
# Expected: no errors, no duplicate symbol for audio_system
```

---

## Summary: File Change List

| # | File | Action | Depends On |
|---|------|--------|------------|
| 1a | `CMakeLists.txt` | Add content copy rules after line 186 | — |
| 1b | `CMakeLists.txt` | Add `--preload-file content@/content` to Emscripten flags | — |
| 2a | `app/beat_map_loader.h` | Add `difficulty` parameter to signatures | — |
| 2b | `app/beat_map_loader.cpp` | Replace `parse_beat_map` + `load_beat_map` bodies | — |
| 3a | `app/components/music.h` | **NEW FILE** — `MusicContext` struct | — |
| 3b | `app/components/rhythm.h` | Add `restart_music` bool to `SongState` | — |
| 3c | `CMakeLists.txt` | Exclude `song_playback_system.cpp` from lib, add to exe | — |
| 4  | `app/main.cpp` | Audio init, beatmap loading, music loading, shutdown | 1, 2, 3 |
| 5a | `app/systems/song_playback_system.cpp` | Full rewrite — authoritative clock + music lifecycle | 3, 4 |
| 5b | `app/systems/game_state_system.cpp` | Add `restart_music = true` in `enter_playing()` | 3b |
| 6a | `app/systems/audio_system.cpp` | **NEW FILE** — AudioQueue drain stub | — |
| 6b | `app/systems/render_system.cpp` | Delete audio_system stub (lines 362–368) | 6a |

## Implementation Order

```
Phase 1 (parallel, no dependencies):
  ├── Change 1: CMake content copy + Emscripten preload
  ├── Change 2: Loader nested difficulties
  ├── Change 3a: music.h new file
  ├── Change 3b: SongState.restart_music field
  ├── Change 6a: audio_system.cpp new file
  └── Change 6b: Remove stub from render_system.cpp

Phase 2 (depends on Phase 1):
  ├── Change 3c: CMake lib/exe split for song_playback_system
  └── Change 4: main.cpp audio init + beatmap loading

Phase 3 (depends on Phase 2):
  ├── Change 5a: song_playback_system rewrite
  └── Change 5b: game_state_system restart_music flag
```

### Build + Test Sequence
```bash
# After all changes:
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/shapeshifter   # verify: music plays, beats sync, restart works

# Run unit tests:
cmake --build build --target shapeshifter_tests
cd build && ctest --output-on-failure
```

---

## Cache & Memory Analysis

| Data | Element Size | Count | Total | Access Pattern | Cache Tier |
|------|-------------|-------|-------|----------------|------------|
| BeatEntry | 8 bytes | 73–256 | 0.6–2 KB | Sequential scan by beat_scheduler | L1 hot during spawn bursts |
| SongState | ~96 bytes | 1 | 96 B | Read every frame by 2 systems | L1 (singleton, stays resident) |
| MusicContext | ~56 bytes | 1 | 56 B | Read 1–2×/frame | L1 (cold singleton) |
| AudioQueue | ~20 bytes | 1 | 20 B | Written by collision/hp, drained by audio_system | L1 |
| BeatMap | ~80 + heap | 1 | ~2 KB | Read by beat_scheduler only | L2 (cold metadata) |

All singletons fit in a single cache line or two. No cache pressure concern. The hot path is beat_scheduler scanning BeatEntry[] — at 8 bytes each, 8 entries fit per cache line, and the sequential access pattern lets the hardware prefetcher run perfectly.
