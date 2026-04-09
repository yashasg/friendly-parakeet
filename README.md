# SHAPESHIFTER

An endless runner where you shift between geometric shapes to pass through obstacles. The core twist: a **Burnout scoring system** rewards waiting until the last possible moment — the longer you delay, the higher your multiplier, but wait too long and you crash.

Built with **C++17**, **SDL2**, and **EnTT** using a strict Data-Oriented Design architecture.

## Gameplay

- **3 shapes** — Circle, Square, Triangle. Tap to shift instantly.
- **3 lanes** — Swipe left/right to strafe. Swipe up to jump, down to slide.
- **6 obstacle types** — Shape gates, lane blocks, low/high bars, combo gates, split paths.
- **Burnout meter** — Acts early = safe but low score. Waits until danger zone = up to ×5 multiplier.
- **Difficulty ramp** — Speed increases from ×1.0 to ×3.0 over 3 minutes. Obstacles get denser and more complex.

## Building

### Prerequisites

- C++17 compiler (Clang or GCC)
- CMake 3.20+
- SDL2 development libraries

Dependencies (EnTT, SDL2, Catch2) are resolved automatically via [vcpkg](https://vcpkg.io) or CMake FetchContent fallback.

### Build

```bash
./build.sh           # Release build
./build.sh Debug     # Debug build
```

### Run

```bash
./run.sh             # Build and run the game
./run.sh test        # Build and run tests
./run.sh bench       # Build and run benchmarks
```

### With vcpkg (optional)

```bash
export VCPKG_ROOT=/path/to/vcpkg
./build.sh
```

If `VCPKG_ROOT` is set, the build uses vcpkg packages. Otherwise it falls back to FetchContent and builds dependencies from source.

## Architecture

The codebase follows **Data-Oriented Design** principles from Richard Fabian's *Data-Oriented Design* book:

- **Components** are plain POD structs with no methods, no inheritance, no virtuals
- **Systems** are free functions: `void system_name(entt::registry& reg, float dt)`
- **No entity base class** — entities are implicit IDs linking rows across component tables
- **Existential processing** — component presence/absence replaces boolean flags
- **All state** lives in the `entt::registry` (entity components + `ctx()` singletons)

### System Pipeline

Each frame runs a fixed-timestep loop executing systems in order:

```
input_system          → polls SDL events into InputState
gesture_system        → classifies touch input into gestures
game_state_system     → manages phase transitions (Title/Playing/Paused/GameOver)
player_action_system  → applies shape changes, lane switches, jumps, slides
player_movement_system → interpolates positions and animations
difficulty_system     → ramps speed, spawn rate, burnout window
obstacle_spawn_system → creates obstacle entities
scroll_system         → moves entities by velocity
burnout_system        → tracks nearest threat, calculates burnout zone
collision_system      → checks obstacle clearance or crash
scoring_system        → awards points with burnout multiplier
lifetime_system       → destroys expired entities
particle_system       → updates particle size and gravity
cleanup_system        → destroys off-screen obstacles
render_system         → draws everything via SDL2
audio_system          → plays queued SFX (stub)
```

### Project Layout

```
app/
├── main.cpp              # SDL init + game loop (calls systems)
├── constants.h           # All tuning values
├── components/           # 13 POD component structs
│   ├── transform.h       #   Position, Velocity
│   ├── player.h          #   PlayerTag, PlayerShape, Lane, VerticalState
│   ├── obstacle.h        #   ObstacleTag, Obstacle, ScoredTag
│   ├── obstacle_data.h   #   RequiredShape, BlockedLanes, RequiredLane, RequiredVAction
│   ├── input.h           #   InputState, GestureResult, ShapeButtonEvent
│   ├── game_state.h      #   GameState, GamePhase
│   ├── scoring.h         #   ScoreState, ScorePopup
│   ├── burnout.h         #   BurnoutState, BurnoutZone
│   ├── difficulty.h      #   DifficultyConfig
│   ├── rendering.h       #   Color, DrawSize, DrawLayer
│   ├── lifetime.h        #   Lifetime
│   ├── particle.h        #   ParticleTag, ParticleData
│   └── audio.h           #   AudioQueue, SFX
└── systems/              # 15 system free functions
    └── all_systems.h     #   declarations + pipeline order
tests/                    # Catch2 unit tests (8 test files)
benchmarks/               # Catch2 micro-benchmarks
design-docs/              # Game design + architecture docs
```

## Testing

Tests use [Catch2](https://github.com/catchorg/Catch2) v3 and cover all gameplay systems:

```bash
./run.sh test                              # Run all tests
./build/shapeshifter_tests "[collision]"    # Run specific tag
./build/shapeshifter_tests --list-tests    # List all test cases
```

## Benchmarks

Micro-benchmarks measure per-system and full-frame performance:

```bash
./run.sh bench
```

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| [SDL2](https://www.libsdl.org/) | 2.30+ | Windowing, rendering, input |
| [EnTT](https://github.com/skypjack/entt) | 3.14+ | Entity Component System |
| [Catch2](https://github.com/catchorg/Catch2) | 3.7+ | Testing and benchmarks |

## License

See repository for license details.
