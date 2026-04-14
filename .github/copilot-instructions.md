# Copilot Instructions — Bullet Hell Game

## Project Overview

A bullet hell game built in C++ using raylib for rendering and EnTT (v3.16.0) for Entity Component System architecture.

## Tech Stack

- **Language:** C++17 (or later)
- **Build system:** CMake
- **Rendering:** raylib
- **ECS framework:** [EnTT](https://github.com/skypjack/entt) v3.16.0
- **Design docs:** `design-docs/` — check here before implementing any feature:
  - `game.md` — GDD: shapes, controls, burnout scoring, obstacles, difficulty
  - `prototype.md` — ASCII art prototype with frame-by-frame gameplay scenarios
  - `architecture.md` — ECS components, system execution order, entity archetypes, main loop
  - `feature-specs.md` — Input system, burnout scoring, obstacle spawning specs with C++ structs
  - `game-flow.md` — Screen flow, tutorial (FTUE), feedback/juice spec, HUD state machine, transitions

## Build Commands

```bash
cmake -B build -S .
cmake --build build
./build/shapeshifter

# Run tests
./build/shapeshifter_tests

# Or via run script
./run.sh           # build + run game
./run.sh test      # build + run tests
```

## Architecture

This project uses an **Entity Component System (ECS)** pattern via EnTT. Follow these principles:

- **Components** are plain data structs (no logic, no methods beyond constructors)
- **Systems** are free functions that operate on component queries via `entt::registry`
- **Entities** are just IDs — never store game state outside the registry
- Keep systems decoupled: a system should query only the components it needs
- Prefer creating new components over adding fields to existing ones when the data serves a different system

### Typical System Signature

```cpp
void update_movement(entt::registry& registry, float dt) {
    auto view = registry.view<Position, Velocity>();
    for (auto [entity, pos, vel] : view.each()) {
        pos.x += vel.dx * dt;
        pos.y += vel.dy * dt;
    }
}
```

## LSP / Code Intelligence

A clangd LSP server is configured in `.github/lsp.json` for C/C++ code intelligence (go-to-definition, references, diagnostics, etc.). The main agent should use clangd for accurate code navigation and pass gathered context to sub-agents, since sub-agents do not have direct LSP access.

## Conventions

- Game source code goes in `app/`
- All game state lives in the `entt::registry` — avoid global mutable state
- Use raylib directly for input, windowing, and rendering (no wrapper libraries)
- Bullet patterns should be data-driven where possible (define patterns as data, not hardcoded logic)

## Zero Warnings Policy

This is a **no-warning shop**. All builds must compile with zero warnings:

- **Clang (native):** `-Wall -Wextra -Werror` — any warning is a build failure
- **MSVC (Windows):** `/W4 /WX` — same policy
- **Emscripten (WASM):** same Clang flags apply
- **CMake:** `-Wno-dev` for configure step (suppresses third-party vcpkg warnings we don't control)

Before submitting code, verify it compiles warning-free on all platforms. Common pitfalls:
- Unused variables (especially behind `#ifdef` platform guards)
- Implicit conversions between signed/unsigned or float/double
- Missing switch cases on enums
