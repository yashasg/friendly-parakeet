# SHAPESHIFTER

![CI (macOS)](https://github.com/yashasg/friendly-parakeet/actions/workflows/ci-macos.yml/badge.svg)
![CI (Linux)](https://github.com/yashasg/friendly-parakeet/actions/workflows/ci-linux.yml/badge.svg)
![CI (Windows)](https://github.com/yashasg/friendly-parakeet/actions/workflows/ci-windows.yml/badge.svg)
![CI (WebAssembly)](https://github.com/yashasg/friendly-parakeet/actions/workflows/ci-wasm.yml/badge.svg)

A rhythm game where obstacles ARE the beats. Match shapes and dodge lane blocks in time with the music — the closer to the beat, the higher your score.

Built with **C++20**, **raylib**, and **EnTT** using Data-Oriented Design.

**[Play in browser](https://yashasg.github.io/friendly-parakeet/)** (WebAssembly)

## Gameplay

- **3 shapes** — Circle, Square, Triangle. Press shape buttons on the beat.
- **3 lanes** — Swipe or press A/D to dodge lane blocks.
- **Timing grades** — Perfect > Good > Ok > Bad. Closer to the beat = higher score.
- **Rhythm windows** — Shape changes are temporary; match the shape before the window closes.
- **Song-driven levels** — Obstacles are generated from audio analysis of the music.

## Building

### Prerequisites

- C++20 compiler (Clang 20 recommended)
- CMake 3.20+
- [vcpkg](https://vcpkg.io)

### Quick Start

```bash
export VCPKG_ROOT=/path/to/vcpkg

./build.sh           # Release build
./run.sh             # Build + run game
./run.sh test        # Build + run tests
```

### Difficulty Selection

```bash
./build/shapeshifter --difficulty easy
./build/shapeshifter --difficulty medium   # default
./build/shapeshifter --difficulty hard
```

### Test Player (AI)

```bash
./build/shapeshifter --test-player pro     # clears with high score
./build/shapeshifter --test-player good    # clears easy/medium
./build/shapeshifter --test-player bad     # struggles on medium+
```

## Controls

| Action | Keyboard | Touch |
|--------|----------|-------|
| Shape: Circle | 1 | Tap left button |
| Shape: Triangle | 2 | Tap center button |
| Shape: Square | 3 | Tap right button |
| Move left | A | Swipe left |
| Move right | D | Swipe right |

## Architecture

**Data-Oriented Design** with EnTT ECS — components are plain structs, systems are free functions.

### System Pipeline (per frame)

```
input -> gesture -> game_state -> song_playback -> beat_scheduler ->
player_action -> shape_window -> player_movement -> difficulty ->
obstacle_spawn -> scroll -> collision -> scoring ->
hp -> lifetime -> particle -> cleanup -> render -> audio
```

### Key Design Decisions

- **Rhythm positions derived from song_time**, not accumulated dt — prevents beat drift
- **Shape windows** are song-time-anchored with phase transitions (MorphIn -> Active -> MorphOut -> Idle)
- **Single-pass collision** dispatches by obstacle kind via switch, not multiple EnTT views
- **Section pattern reuse** — verse 1 and verse 2 share the same obstacle pattern

### Project Layout

```
app/
  main.cpp                # raylib init, game loop, Emscripten support
  constants.h             # All tuning values
  platform.h              # PLATFORM_HAS_KEYBOARD macro
  platform_utils.h        # Portable localtime/fopen wrappers
  enum_names.h            # shape_name(), obstacle_kind_name()
  components/             # 19 POD component structs
    player.h              #   PlayerShape, ShapeWindow, Lane
    rhythm.h              #   TimingGrade, BeatInfo, WindowPhase
    beat_map.h            #   BeatEntry, BeatMap (loaded data)
    song_state.h          #   SongState, HPState, SongResults
  systems/                # 23 system functions
    all_systems.h         #   declarations + pipeline order
    play_session.cpp      #   entity setup on game start
tools/
  rhythm_pipeline.py      # Audio analysis (aubio -> analysis JSON)
  level_designer.py       # Beatmap generation (analysis -> beatmap JSON)
content/
  audio/                  # Source audio files (.flac)
  beatmaps/               # Analysis + beatmap JSON files
tests/                    # 386 Catch2 tests across 19 files
design-docs/              # Game design + architecture docs
```

## Beatmap Pipeline

Levels are generated from audio analysis:

```bash
# 1. Analyze audio
python3 tools/rhythm_pipeline.py content/audio/song.flac \
    --output content/beatmaps/song_analysis.json

# 2. Generate beatmap (easy/medium/hard)
python3 tools/level_designer.py content/beatmaps/song_analysis.json \
    --output content/beatmaps/song_beatmap.json
```

The level designer uses song structure (intro/verse/chorus/bridge) to control density and obstacle variety. Shapes are assigned for flow — short gaps repeat shapes, long gaps alternate.

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| [raylib](https://www.raylib.com/) | 5.5 | Rendering, input, audio |
| [EnTT](https://github.com/skypjack/entt) | 3.16.0 | Entity Component System |
| [nlohmann-json](https://github.com/nlohmann/json) | 3.12+ | Beatmap JSON parsing |
| [Catch2](https://github.com/catchorg/Catch2) | 3.13+ | Testing framework |

## CI/CD

Builds on every push to main across 3 platforms:

| Platform | Compiler | Notes |
|----------|----------|-------|
| Linux | Clang 20 | Native + WebAssembly |
| macOS | Clang 20 (Homebrew LLVM) | Native |
| Windows | Clang 20 (pre-installed) | Native |

WebAssembly builds auto-deploy to GitHub Pages on merge to main, and the beatmap/level editor is published at `https://yashasg.github.io/friendly-parakeet/level-editor/`.
