# SDL2 → raylib Migration Plan

## Overview

Migrate the rendering, windowing, input, timing, and audio platform layer from
SDL2 + SDL2_ttf to raylib 5.5. All ECS components and game-logic systems are
unaffected. The `TextContext` / `text_draw` API boundary is preserved — only
the implementation behind it changes.

**Target platforms after migration:**

| Platform | Backend | Notes |
|---|---|---|
| macOS | OpenGL 3.3 (via GLFW) | Metal not used — raylib defaults to GL on macOS |
| Windows x64 | OpenGL 3.3 (via GLFW) | D3D11 not available in raylib |
| Linux | OpenGL 3.3 (via GLFW) | Same as current SDL2 backend |
| Web | WebGL2 (Emscripten) | Requires `emscripten_set_main_loop` shim |

---

## Dependency

**Chosen approach: vcpkg with the GitHub Actions binary cache.**

Your CI already has `VCPKG_BINARY_SOURCES: "clear;x-gha,readwrite"` set in
both `ci-linux.yml` and `ci-windows.yml`, along with the required
`ACTIONS_CACHE_URL` / `ACTIONS_RUNTIME_TOKEN` exports. This means vcpkg
uploads each compiled package to the GHA cache after the first build, and
restores it on every subsequent run — **no recompilation on cache hit**.

The economics per platform:

| Platform | Cold build (first run) | Warm build (cache hit) |
|---|---|---|
| macOS | ~90s (raylib + GLFW compile) | ~5s (cache restore) |
| Windows x64 | ~2 min (MSVC, raylib + GLFW) | ~5s |
| Linux x64 | ~90s (clang, raylib + GLFW) | ~5s |
| Web (Emscripten) | ❌ no vcpkg emscripten triplet — see below | — |

The cache key is content-addressed on the package name + version + triplet +
compiler ABI hash. Bumping `"version": "5.5"` in `vcpkg.json` automatically
invalidates the old entry and triggers one rebuild, which then re-populates
the cache.

**Why not NuGet?** The [`raylib` NuGet package](https://www.nuget.org/packages/raylib)
is Windows-only (x86 + x64 DLLs), ships a C++ wrapper layer (`raylib-cpp`),
and integrates via MSBuild `.targets` — not CMake. It has no macOS, Linux, or
Web artifacts.

**Why not `FetchContent` prebuilt binaries?** Downloading the official release
tarballs via `FetchContent URL` avoids compilation but couples you to the
exact compiler + CRT that Raymond used to build the official binaries. On
Windows the official archive is an MSVC shared lib (`raylib.dll` +
`raylib.lib`) — no static option. On Linux the official `.a` is compiled with
GCC; mixing that with your Clang CI (which uses `-stdlib=libc++`) can cause
ABI issues. vcpkg builds with your exact toolchain, so the ABI always matches.

**Emscripten — build from source, same as raylib's own CI.** raylib's
[`build_webassembly.yml`](https://github.com/raysan5/raylib/blob/master/.github/workflows/build_webassembly.yml)
builds the wasm archive by running `make PLATFORM=PLATFORM_WEB` with
`mymindstorm/setup-emsdk@v14` pinned to emsdk `5.0.3` on `windows-latest`.
There is no magic — it is a plain source compile with the Emscripten toolchain.
We can do exactly the same: pull raylib source via `FetchContent GIT_TAG 5.5`
and let CMake compile it as part of the Emscripten build. Since this only runs
on the dedicated `ci-web` job (not the native jobs), the compile cost is paid
once per `vcpkg.json`-equivalent change, and the GHA cache for the web job
covers subsequent runs via the `actions/cache` step on the build directory.

`vcpkg.json` after migration:

```json
{
  "dependencies": [
    "entt",
    { "name": "catch2", "platform": "!emscripten" },
    { "name": "raylib", "platform": "!emscripten" }
  ]
}
```

The existing `VCPKG_DEFAULT_TRIPLET: x64-windows-static-md` in
`ci-windows.yml` gives a fully-static Windows build (no `raylib.dll` to ship).
No CI workflow changes are needed to enable caching — it is already live.

---

## Phase 0 — Dependency swap + build system

**Branch:** `refactor/raylib-deps`  
**Files:** `CMakeLists.txt`, `vcpkg.json`

### CMakeLists.txt — raylib target

Replace all SDL2 `find_package` / target-alias blocks with:

```cmake
if(NOT EMSCRIPTEN)
    # vcpkg builds raylib from source on first run and caches the binary via
    # VCPKG_BINARY_SOURCES=x-gha,readwrite — subsequent CI runs restore from
    # cache and skip compilation entirely.
    find_package(raylib CONFIG REQUIRED)
else()
    # No vcpkg emscripten triplet for raylib — build from source.
    # Mirrors raylib's own build_webassembly.yml exactly.
    include(FetchContent)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        5.5
        GIT_SHALLOW    ON
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
endif()
```

`find_package(raylib CONFIG REQUIRED)` works because vcpkg installs a
`raylib-config.cmake` alongside the built lib. The `VCPKG_DEFAULT_TRIPLET:
x64-windows-static-md` already set in `ci-windows.yml` ensures the Windows
build is fully static — no `raylib.dll` to ship.

### CMakeLists.txt — target changes

- `shapeshifter_lib`: remove `SDL2::SDL2-static` from public link. `EnTT::EnTT`
  only.
- `shapeshifter` executable: replace `SDL2::SDL2main` + `SDL2_ttf::SDL2_ttf`
  with `raylib`.
- `shapeshifter_tests`: no change.
- Wrap test target in `if(NOT EMSCRIPTEN)`.
- Remove the SDL2 target-name normalisation aliases.
- Keep `PLATFORM_DESKTOP` definition — redefine it for `APPLE`, `WIN32`, and
  non-`EMSCRIPTEN` Linux (same requirement as sokol plan).
- For Emscripten, add linker flags:

```cmake
if(EMSCRIPTEN)
    target_link_options(shapeshifter PRIVATE
        -sUSE_GLFW=3
        -sUSE_WEBGL2=1
        -sALLOW_MEMORY_GROWTH=1
        --shell-file ${CMAKE_SOURCE_DIR}/app/web/shell.html
        --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets)
endif()
```

---

## Phase 1 — Main loop rewrite (`app/main.cpp`)

**Branch:** `refactor/raylib-main-loop`  
**Files:** `app/main.cpp`, `app/systems/all_systems.h`

### Structural change

raylib, like SDL2, uses a **blocking `while` loop** — no callback inversion.
This is the most significant difference from the sokol plan: `main()` stays
structurally intact.

```cpp
// Before (SDL2)
SDL_Init → SDL_CreateWindow → SDL_CreateRenderer
while(true) { SDL_PollEvent → tick → render }
SDL_DestroyRenderer → SDL_DestroyWindow → SDL_Quit

// After (raylib)
InitWindow(W, H, title)
SetTargetFPS(60)
while (!WindowShouldClose()) {
    float raw_dt = GetFrameTime();
    // tick systems...
    BeginDrawing();
        render_system(reg, alpha);
    EndDrawing();
}
CloseWindow();
```

On **Emscripten**, `while(!WindowShouldClose())` does not work — the browser
event loop is not blocking. raylib provides a shim:

```cpp
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(update_draw_frame, 0, 1);
#else
    while (!WindowShouldClose()) { update_draw_frame(); }
#endif
```

`update_draw_frame` is a static free function containing one iteration of the
game loop body. The `entt::registry` and timing state must be accessible to
it — either as a file-scoped struct or via a static pointer.

### Timing

Replace `SDL_GetPerformanceCounter` / `SDL_GetPerformanceFrequency` with:

```cpp
float raw_dt = GetFrameTime();   // seconds since last frame, capped internally
```

Note: `GetFrameTime()` is clamped by raylib's internal FPS cap. The existing
`MAX_ACCUM` guard in the fixed-timestep accumulator still applies.

### Quit

`WindowShouldClose()` returns `true` on window close button or `ESC` key. The
existing `input.quit_requested` flag can be set there, or the `while`
condition can be used directly. Either way, no explicit `SDL_QUIT` event
handling is needed.

---

## Phase 2 — Input system (`app/systems/input_system.cpp`)

**Branch:** `refactor/raylib-input`  
**Files:** `app/systems/input_system.cpp`

### Polling model

raylib uses a **polling model** (same concept as SDL, different API). Input
state is updated internally by `BeginDrawing`/`EndDrawing` — you poll it with
query functions rather than processing an event queue. This means `input_system`
retains its current structure: called once per frame, writes into `InputState`.
No event callback split required.

### Input translation

```cpp
void input_system(entt::registry& reg, float raw_dt) {
    auto& input = reg.ctx().get<InputState>();
    clear_input_events(input);

    // Mouse (desktop)
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        input.touch_down = true;
        input.touching   = true;
        auto pos = GetMousePosition();
        input.start_x = input.curr_x = pos.x;
        input.start_y = input.curr_y = pos.y;
        input.duration = 0.0f;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        input.touch_up = true;
        input.touching = false;
        auto pos = GetMousePosition();
        input.end_x = pos.x; input.end_y = pos.y;
    }
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && input.touching) {
        auto pos = GetMousePosition();
        input.curr_x = pos.x; input.curr_y = pos.y;
    }

    // Touch (mobile / web)
    if (GetTouchPointCount() > 0) {
        // Mirror same logic using GetTouchPosition(0)
    }

#ifdef PLATFORM_DESKTOP
    // Keyboard — IsKeyPressed fires exactly once per press (no repeat)
    if (IsKeyPressed(KEY_W)) input.key_w = true;
    if (IsKeyPressed(KEY_A)) input.key_a = true;
    if (IsKeyPressed(KEY_S)) input.key_s = true;
    if (IsKeyPressed(KEY_D)) input.key_d = true;
    if (IsKeyPressed(KEY_ONE))   input.key_1 = true;
    if (IsKeyPressed(KEY_TWO))   input.key_2 = true;
    if (IsKeyPressed(KEY_THREE)) input.key_3 = true;
#endif

    // Background / suspend — raylib has no direct equivalent to
    // SDL_APP_WILLENTERBACKGROUND. Use IsWindowFocused() as a proxy:
    if (!IsWindowFocused() && reg.ctx().get<GameState>().phase == GamePhase::Playing) {
        auto& gs = reg.ctx().get<GameState>();
        gs.transition_pending = true;
        gs.next_phase = GamePhase::Paused;
    }

    if (input.touching) input.duration += raw_dt;
}
```

Key differences from SDL:
- `IsKeyPressed` already suppresses repeat — no `repeat == 0` guard needed
- Touch coordinates from `GetTouchPosition` are in **screen pixels** (same as
  SDL mouse, unlike SDL's normalised finger events)
- No `SDL_FINGERDOWN` / normalisation by `SCREEN_W` needed

### `all_systems.h` — signature change

```cpp
// Before
struct SDL_Renderer;
void render_system(entt::registry& reg, SDL_Renderer* renderer, float alpha);

// After
void render_system(entt::registry& reg, float alpha);
```

---

## Phase 3 — Render system (`app/systems/render_system.cpp`)

**Branch:** `refactor/raylib-render`  
**Files:** `app/systems/render_system.cpp`

### API mapping

| SDL2 call | raylib replacement |
|---|---|
| `SDL_SetRenderDrawColor(r, R,G,B,A)` | pass `(Color){R,G,B,A}` inline to each draw call |
| `SDL_RenderFillRectF(r, &rect)` | `DrawRectangleRec((Rectangle){x,y,w,h}, color)` |
| `SDL_RenderDrawRectF(r, &rect)` | `DrawRectangleLinesEx((Rectangle){x,y,w,h}, 1.0f, color)` |
| `SDL_RenderDrawLineF(r, x1,y1,x2,y2)` | `DrawLineV((Vector2){x1,y1}, (Vector2){x2,y2}, color)` |
| `SDL_SetRenderDrawBlendMode(BLEND)` | `BeginBlendMode(BLEND_ALPHA)` / `EndBlendMode()` |
| `SDL_RenderClear(r)` | `ClearBackground(color)` |
| `SDL_RenderPresent(r)` | `EndDrawing()` (called in main loop, not in render_system) |
| `SDL_RenderSetLogicalSize` | `BeginMode2D` with a `Camera2D` at offset/zoom, or just use `GetScreenWidth/Height` |

Note: Unlike SDL2 (stateful color) and sokol_gp (stateful color), raylib
passes color **per draw call**. The `SDL_SetRenderDrawColor` + draw pattern
collapses into a single call. This is a cleaner API but requires threading
color through every helper function signature.

### `draw_shape` helper — signature change

```cpp
// Before
static void draw_shape(SDL_Renderer* r, Shape shape, float cx, float cy, float size)

// After
static void draw_shape(Shape shape, float cx, float cy, float size, Color color)
```

This is the **only helper signature that changes**. All internal SDL calls
are replaced with raylib equivalents:

- `Shape::Square` — `DrawRectangleRec`
- `Shape::Triangle` — `DrawTriangle(v1, v2, v3, color)` — direct replacement,
  same concept as `sgp_draw_filled_triangle`
- `Shape::Circle` — `DrawCircleV((Vector2){cx,cy}, radius, color)` — raylib
  has a native filled circle; the scanline loop is eliminated entirely

### Frame structure

`render_system` sits between `BeginDrawing()` and `EndDrawing()` in the main
loop. It does not call either:

```cpp
// main loop
BeginDrawing();
    render_system(reg, alpha);   // ← no renderer param, no present
EndDrawing();                    // ← swaps buffers, polls events
```

### Blend mode overlays

```cpp
BeginBlendMode(BLEND_ALPHA);
DrawRectangle(0, 0, constants::SCREEN_W, constants::SCREEN_H, (Color){0,0,0,180});
EndBlendMode();
```

### Logical size / resolution independence

SDL2 had `SDL_RenderSetLogicalSize` which scaled all drawing to a virtual
canvas. raylib has no direct equivalent. Options:

1. **`Camera2D` with scale** — `BeginMode2D(camera)` / `EndMode2D()` around all
   game-world drawing, with `camera.zoom = screenH / SCREEN_H`.
2. **`BeginScissorMode` + manual scaling** — more involved.
3. **Accept native resolution** — simplest. Since `constants::SCREEN_W/H` are
   used for all coordinate math, just ensure `GetScreenWidth/Height` match or
   add a scale factor. For a mobile/web game this matters more.

This is the only significant design decision in Phase 3 that has no 1:1 SDL
equivalent.

---

## Phase 4 — Text rendering (`app/text_renderer.h/.cpp`)

**Branch:** `refactor/raylib-text`  
**Files:** `app/text_renderer.h`, `app/text_renderer.cpp`

raylib has a complete built-in TTF font system. This is the phase where raylib
has the clearest advantage over both SDL2_ttf and the stb_truetype-from-scratch
approach required by sokol.

### TextContext — new internals

```cpp
struct TextContext {
    Font font_small;    // LoadFontEx at small pixel size
    Font font_medium;   // LoadFontEx at medium pixel size
    Font font_large;    // LoadFontEx at large pixel size
    // No atlas management, no sg_image, no stbtt_packedchar
};
```

### Initialisation (`text_init`)

```cpp
ctx.font_small  = LoadFontEx(path, 16, nullptr, 0);
ctx.font_medium = LoadFontEx(path, 28, nullptr, 0);
ctx.font_large  = LoadFontEx(path, 48, nullptr, 0);
```

One line per size. raylib handles atlas creation, GPU upload, and glyph
packing internally.

### Drawing (`text_draw`)

```cpp
// text_draw implementation
Font& font = select_font(ctx, font_size);
Vector2 size = MeasureTextEx(font, text, font.baseSize, 1.0f);
Vector2 pos = {x, y};
if (align == TextAlign::Center) pos.x -= size.x / 2.0f;
if (align == TextAlign::Right)  pos.x -= size.x;
DrawTextEx(font, text, pos, font.baseSize, 1.0f, (Color){r,g,b,a});
```

The `SDL_Renderer*` parameter is removed (same as the sokol plan) — raylib
has no renderer handle.

### Shutdown (`text_shutdown`)

```cpp
UnloadFont(ctx.font_small);
UnloadFont(ctx.font_medium);
UnloadFont(ctx.font_large);
```

No `TTF_Quit`, no atlas cleanup.

---

## Phase 5 — Audio (`audio_system`)

**Branch:** `refactor/raylib-audio`  
**Files:** `app/systems/render_system.cpp`

raylib bundles **miniaudio** as its audio backend. It works on all target
platforms including Web (via Emscripten's OpenAL/Web Audio bridge).

```cpp
// init_cb equivalent — before the game loop:
InitAudioDevice();

// audio_system body (replaces the stub):
void audio_system(entt::registry& reg) {
    auto& audio = reg.ctx().get<AudioQueue>();
    for (int i = 0; i < audio.count; ++i) {
        PlaySound(loaded_sounds[audio.queue[i]]);
    }
    audio_clear(audio);
}

// shutdown:
CloseAudioDevice();
```

Sound assets are loaded once at startup with `LoadSound("assets/sfx/hit.wav")`.
No streaming callback required — raylib handles the audio thread internally.

This is **significantly simpler** than the `sokol_audio.h` push-model, which
requires a stream callback and manual sample management.

---

## Phase 6 — Web CI job (new)

**Branch:** `refactor/raylib-web-ci`  
**Files:** `.github/workflows/ci-web.yml` (new), `app/web/shell.html` (new)

Mirrors raylib's own [`build_webassembly.yml`](https://github.com/raysan5/raylib/blob/master/.github/workflows/build_webassembly.yml):
`mymindstorm/setup-emsdk@v14` pinned to emsdk `5.0.3`, then a standard
`emcmake cmake` configure + build. `FetchContent` clones raylib at `GIT_TAG 5.5`
and compiles it as part of the game's own build — no separate raylib build step
needed. A `actions/cache` step on the build directory covers warm runs.

```yaml
name: CI (Web)
runs-on: ubuntu-latest
steps:
  - uses: actions/checkout@v4

  - uses: mymindstorm/setup-emsdk@v14
    with:
      version: 5.0.3                   # same version as raylib's own CI
      actions-cache-folder: emsdk-cache

  - name: Cache build directory
    uses: actions/cache@v5
    with:
      path: build-web
      key: cmake-web-${{ hashFiles('CMakeLists.txt', 'vcpkg.json') }}
      restore-keys: cmake-web-

  - name: Build (Emscripten)
    run: |
      emcmake cmake -B build-web -S . \
        -DCMAKE_BUILD_TYPE=Release
      cmake --build build-web

  - name: Upload artifact
    uses: actions/upload-artifact@v4
    with:
      name: shapeshifter-web
      path: build-web/shapeshifter.*
```

The `actions/cache` key hashes `CMakeLists.txt` — bumping the raylib `GIT_TAG`
automatically invalidates it and triggers a fresh compile, then re-populates
the cache for subsequent runs. Cold build (raylib + game) ~2–3 min; warm ~30s.

---

## Phase 7 — Linux CI cleanup

Remove `libsdl2-dev`, `libsdl2-ttf-dev` from the `apt-get` step in
`ci-linux.yml`. vcpkg's raylib port pulls in and builds GLFW itself, so no
`libglfw3-dev` is needed either. The only system packages raylib requires on
Linux are the OpenGL and X11 dev headers, which are present on `ubuntu-latest`
by default but are worth pinning explicitly:

```yaml
- name: Install system libs
  run: sudo apt-get install -y libgl1-mesa-dev libx11-dev
```

`catch2` can also be removed from `apt-get` — it is already in `vcpkg.json`
and will be restored from the GHA binary cache like any other vcpkg package.

---

## Files touched — summary

| File | Change |
|---|---|
| `CMakeLists.txt` | Medium — swap deps, add FetchContent prebuilt-binary block + IMPORTED target |
| `vcpkg.json` | Remove sdl2/sdl2-ttf; add raylib with `"platform": "!emscripten"` |
| `app/main.cpp` | Light — SDL_Init/CreateWindow/CreateRenderer → InitWindow; loop body intact |
| `app/systems/all_systems.h` | Remove SDL_Renderer forward decl, update render_system sig |
| `app/systems/render_system.cpp` | Medium — draw calls replaced; color model change |
| `app/systems/input_system.cpp` | Medium — SDL_PollEvent loop → polling calls; structure preserved |
| `app/text_renderer.h` | Swap internals, preserve public API; remove SDL_Renderer param |
| `app/text_renderer.cpp` | Light rewrite — SDL2_ttf → LoadFontEx/DrawTextEx |
| `app/web/shell.html` | New — Emscripten HTML shell |
| `.github/workflows/ci-linux.yml` | Remove SDL system dep install |
| `.github/workflows/ci-windows.yml` | Remove SDL/SDL_ttf references |
| `.github/workflows/ci-web.yml` | New — Emscripten build job |

**No vendor directory.** No impl TUs. No manual atlas management.  
**Unaffected:** All ECS components, all game-logic systems, all tests.

---

## Execution order

```
Phase 0 — CMakeLists + FetchContent prebuilts  → CI must pass on all 3 native platforms
Phase 1 — main loop            → game runs (no rendering yet, black window)
Phase 2 — input                → input works, game logic ticks
Phase 3 — render               → game is visually complete
Phase 4 — text                 → HUD and overlays readable
Phase 5 — audio                → playable with sound (simpler than sokol)
Phase 6 — web CI               → web build verified on every PR
Phase 7 — Linux CI cleanup     → remove SDL system deps from CI
```
