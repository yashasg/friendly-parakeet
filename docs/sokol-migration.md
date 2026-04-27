# SDL2 → Sokol Migration Plan

## Overview

Migrate the rendering, windowing, input, timing, and audio platform layer from
SDL2 + SDL2_ttf to the sokol single-header library family. All ECS components
and game-logic systems are unaffected. The `TextContext` / `text_draw` API
boundary is preserved — only the implementation behind it changes.

**Target platforms after migration:**

| Platform | Backend | sokol define |
|---|---|---|
| macOS | Metal | `SOKOL_METAL` |
| Windows x64 | D3D11 | `SOKOL_D3D11` |
| Linux | OpenGL 3.3 | `SOKOL_GLCORE` |
| Web | WebGL2 (Emscripten) | `SOKOL_GLES3` |

---

## Vendor Set

All dependencies are **vendored** — copied as source into the repo at a pinned
commit. No FetchContent, no vcpkg entries for these headers.

```
app/external/sokol/sokol_app.h          # floooh/sokol @ <commit>
app/external/sokol/sokol_gfx.h          # floooh/sokol @ <commit>
app/external/sokol/sokol_glue.h         # floooh/sokol @ <commit>
app/external/sokol/sokol_log.h          # floooh/sokol @ <commit>
app/external/sokol/sokol_time.h         # floooh/sokol @ <commit>
app/external/sokol/sokol_audio.h        # floooh/sokol @ <commit>
app/external/sokol/sokol_debugtext.h    # floooh/sokol @ <commit>
app/external/sokol_gp/sokol_gp.h        # edubart/sokol_gp @ <commit>
app/external/stb/stb_truetype.h         # nothings/stb @ <commit>
app/external/stb/stb_rect_pack.h        # nothings/stb @ <commit>
```

Document the exact commit hash for each in a `app/external/VERSIONS.md` file.
When updating a vendored header, do it as an isolated commit with the hash in
the message:

```
vendor: update sokol to a1b2c3d4
```

### sokol impl translation units

Each sokol header requires exactly one `.cpp` file that defines `SOKOL_IMPL`
before the include. These live in `app/external/sokol/impl/`:

```
app/external/sokol/impl/sokol_app.cpp
app/external/sokol/impl/sokol_gfx.cpp
app/external/sokol/impl/sokol_glue.cpp
app/external/sokol/impl/sokol_log.cpp
app/external/sokol/impl/sokol_time.cpp
app/external/sokol/impl/sokol_audio.cpp
app/external/sokol/impl/sokol_debugtext.cpp
app/external/sokol_gp/impl/sokol_gp.cpp
app/external/stb/impl/stb_impl.cpp      # both stb headers in one TU
```

Each file is ~5 lines, e.g.:

```cpp
// sokol_gfx.cpp
#define SOKOL_IMPL
#include "../sokol_gfx.h"
```

---

## Phase 0 — Vendor headers + build system

**Branch:** `refactor/sokol-vendor`  
**Files:** `CMakeLists.txt`, `vcpkg.json`, `app/external/`

### vcpkg.json

Remove `sdl2` and `sdl2-ttf`. Keep `entt` and `catch2` (for non-Emscripten
builds):

```json
{
  "dependencies": [
    "entt",
    { "name": "catch2", "platform": "!emscripten" }
  ]
}
```

### CMakeLists.txt — sokol_impl target

Replace all SDL2 `find_package` / FetchContent blocks with:

```cmake
# ── sokol impl library ───────────────────────────────────────
file(GLOB SOKOL_IMPL_SOURCES
    app/external/sokol/impl/*.cpp
    app/external/sokol_gp/impl/*.cpp
    app/external/stb/impl/*.cpp
)
add_library(sokol_impl STATIC ${SOKOL_IMPL_SOURCES})
target_include_directories(sokol_impl PUBLIC
    app/external/sokol
    app/external/sokol_gp
    app/external/stb
)

if(APPLE)
    target_compile_definitions(sokol_impl PUBLIC SOKOL_METAL)
    target_link_libraries(sokol_impl PUBLIC
        "-framework Metal"
        "-framework QuartzCore"
        "-framework Cocoa"
        "-framework AudioToolbox")
elseif(WIN32)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "Only 64-bit Windows builds are supported")
    endif()
    target_compile_definitions(sokol_impl PUBLIC SOKOL_D3D11)
    target_link_libraries(sokol_impl PUBLIC d3d11 dxgi dxguid)
elseif(EMSCRIPTEN)
    target_compile_definitions(sokol_impl PUBLIC SOKOL_GLES3)
    target_link_options(shapeshifter PRIVATE
        -sUSE_WEBGL2=1
        -sALLOW_MEMORY_GROWTH=1
        --shell-file ${CMAKE_SOURCE_DIR}/app/web/shell.html
        --preload-file ${CMAKE_SOURCE_DIR}/assets@/assets)
else()  # Linux
    target_compile_definitions(sokol_impl PUBLIC SOKOL_GLCORE)
    target_link_libraries(sokol_impl PUBLIC GL X11 Xi Xcursor Xrandr dl pthread)
endif()
```

### CMakeLists.txt — target changes

- `shapeshifter_lib`: remove `SDL2::SDL2-static` from public link.
  `EnTT::EnTT` only. No SDL headers anywhere in the game-logic layer.
- `shapeshifter` executable: replace `SDL2::SDL2main` + `SDL2_ttf::SDL2_ttf`
  with `sokol_impl`.
- `shapeshifter_tests`: no change — already had no SDL dependency.
- Wrap the test target in `if(NOT EMSCRIPTEN)`.
- Remove the SDL2 target-name normalisation aliases.
- Keep the `PLATFORM_DESKTOP` compile definition. Redefine it based on the
  sokol target rather than SDL: set it for `APPLE`, `WIN32`, and non-`EMSCRIPTEN`
  Linux. Both `input_system.cpp` and `gesture_system.cpp` gate keyboard paths
  on this flag and must continue to compile correctly.
- Remove the font asset `add_custom_command` — fonts are now embedded or
  loaded via Emscripten's virtual FS.

### Linux CI

Remove the `apt-get` install of `libsdl2-dev`, `libsdl2-ttf-dev`. Keep
`catch2` system package or switch to vcpkg-provided. Remove the SDL system
dependency install step entirely.

---

## Phase 1 — Main loop rewrite (`app/main.cpp`)

**Branch:** `refactor/sokol-main-loop`  
**Files:** `app/main.cpp`, `app/systems/all_systems.h`

### Structural change

SDL2 uses a blocking `int main()` with a `while(true)` loop.  
sokol_app inverts control: you provide callbacks in `sapp_desc` and return
`sokol_main()`.

```
// Before
int main() {
    SDL_Init → SDL_CreateWindow → SDL_CreateRenderer
    while(true) { input → tick → render → audio }
    SDL_DestroyRenderer → SDL_DestroyWindow → SDL_Quit
}

// After
static AppState g_app;   // file-scoped, not global game state

static void init()    { sg_setup; sgp_setup; stm_setup; saudio_setup; registry init }
static void frame()   { stm_now timing; input tick; fixed accumulator; render; audio }
static void event(const sapp_event* e) { translate e → InputState writes }
static void cleanup() { text_shutdown; sgp_shutdown; sg_shutdown; saudio_shutdown }

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){
        .init_cb    = init,
        .frame_cb   = frame,
        .event_cb   = event,
        .cleanup_cb = cleanup,
        .width      = constants::SCREEN_W / 2,
        .height     = constants::SCREEN_H / 2,
        .window_title = "SHAPESHIFTER",
        .logger.func  = slog_func,
    };
}
```

`AppState` is a file-scoped struct in `main.cpp` containing the
`entt::registry` and the sokol timing state. It must never be accessed from
any system — systems already receive the registry by reference.

### Timing

Replace `SDL_GetPerformanceCounter` / `SDL_GetPerformanceFrequency` with:

```cpp
stm_setup();   // once in init_cb
// in frame_cb:
static uint64_t last_time = 0;
float raw_dt = (float)stm_sec(stm_laptime(&last_time));
```

### Web quit guard

`sapp_quit()` is a no-op in the browser. The quit path must be guarded:

```cpp
// in event_cb, on SAPP_EVENTTYPE_QUIT_REQUESTED:
#ifndef __EMSCRIPTEN__
    sapp_quit();
#endif
```

---

## Phase 2 — Input system (`app/systems/input_system.cpp`)

**Branch:** `refactor/sokol-input`  
**Files:** `app/systems/input_system.cpp`, `app/main.cpp`

### Split responsibility

The SDL `SDL_PollEvent` loop moves entirely into the `event_cb` in `main.cpp`.
`input_system` becomes a pure tick function — it only advances `input.duration`
and can be renamed `input_tick_system` to make the split explicit.

### Event translation table

| `sapp_event` type | `InputState` writes |
|---|---|
| `SAPP_EVENTTYPE_MOUSE_DOWN` | `touch_down=true`, `touching=true`, `start_x/y`, `curr_x/y`, `duration=0` |
| `SAPP_EVENTTYPE_MOUSE_UP` | `touch_up=true`, `touching=false`, `end_x/y` |
| `SAPP_EVENTTYPE_MOUSE_MOVED` | `curr_x/y` (if `touching`) |
| `SAPP_EVENTTYPE_TOUCHES_BEGAN` | same as MOUSE_DOWN via `touches[0].pos_x/y` |
| `SAPP_EVENTTYPE_TOUCHES_MOVED` | `curr_x/y` |
| `SAPP_EVENTTYPE_TOUCHES_ENDED` | `touch_up`, `end_x/y` |
| `SAPP_EVENTTYPE_SUSPENDED` | transition to `GamePhase::Paused` |
| `SAPP_EVENTTYPE_QUIT_REQUESTED` | `quit_requested=true` (non-web only) |
| `SAPP_EVENTTYPE_KEY_DOWN` (PLATFORM_DESKTOP) | map `e.key_code` → `key_w/a/s/d/1/2/3`; guard `if (!e.key_repeat)` to match SDL's `repeat == 0` behaviour |
| `SAPP_EVENTTYPE_KEY_UP` | not currently used — no field in InputState for held keys |

Touch coordinates from sokol_app are already in pixels (not normalised 0–1 as
in SDL). Verify against `sapp_width()` / `sapp_height()` for DPI-scaled
displays and apply `sapp_dpi_scale()` if needed.

### `all_systems.h` — signature change

```cpp
// Before
struct SDL_Renderer;
void render_system(entt::registry& reg, SDL_Renderer* renderer, float alpha);

// After
void render_system(entt::registry& reg, float alpha);
```

Remove the `struct SDL_Renderer;` forward declaration entirely.

---

## Phase 3 — Render system (`app/systems/render_system.cpp`)

**Branch:** `refactor/sokol-render`  
**Files:** `app/systems/render_system.cpp`

### API mapping

| SDL2 call | sokol_gp replacement |
|---|---|
| `SDL_SetRenderDrawColor(r, R,G,B,A)` | `sgp_set_color(R/255.f, G/255.f, B/255.f, A/255.f)` |
| `SDL_RenderFillRectF(r, &rect)` | `sgp_draw_filled_rect(x, y, w, h)` |
| `SDL_RenderDrawRectF(r, &rect)` | 4× `sgp_draw_line` (top, right, bottom, left edges) — sokol_gp has no outline-rect primitive |
| `SDL_RenderDrawLineF(r, x1,y1,x2,y2)` | `sgp_draw_line(x1, y1, x2, y2)` |
| `SDL_SetRenderDrawBlendMode(BLEND)` | `sgp_set_blend_mode(SGP_BLENDMODE_BLEND)` |
| `SDL_RenderClear(r)` | `sgp_clear()` after `sgp_set_color(...)` |
| `SDL_RenderPresent(r)` | removed — `sg_commit()` is in `frame_cb` |
| `SDL_RenderSetLogicalSize` | `sgp_project(0, SCREEN_W, 0, SCREEN_H)` — signature is `(left, right, top, bottom)` |

### Frame structure

`render_system` no longer calls present/commit. The frame_cb owns that:

```cpp
// frame_cb in main.cpp
int w = sapp_width(), h = sapp_height();
sg_pass pass = { .swapchain = sglue_swapchain() };
sg_begin_pass(&pass);
sgp_begin(w, h);
sgp_viewport(0, 0, w, h);
// left=0, right=SCREEN_W, top=0, bottom=SCREEN_H (Y grows downward, matching SDL)
sgp_project(0.0f, (float)constants::SCREEN_W, 0.0f, (float)constants::SCREEN_H);

render_system(reg, alpha);   // ← no renderer param

sgp_flush();
sgp_end();
sg_end_pass();
sg_commit();
```

### `draw_shape` helper

- `Shape::Square` — `sgp_draw_filled_rect`
- `Shape::Triangle` — `sgp_draw_filled_triangle(x1,y1, x2,y2, x3,y3)` using
  the three apex coordinates; eliminates the scanline loop entirely
- `Shape::Circle` — keep the existing scanline approach: loop rows of
  `sgp_draw_filled_rect` (same logic as current SDL impl). sokol_gp has no
  filled-circle or polygon primitive. The low-level `sgp_draw` with
  `SG_PRIMITIVETYPE_TRIANGLE_STRIP` is an option for a higher-quality circle
  but is out of scope for this migration.

### Blend mode overlays

The game-over and pause dim overlays use `SDL_BLENDMODE_BLEND`. In sokol_gp:

```cpp
sgp_set_blend_mode(SGP_BLENDMODE_BLEND);
sgp_set_color(0, 0, 0, 0.7f);
sgp_draw_filled_rect(0, 0, constants::SCREEN_W, constants::SCREEN_H);
sgp_set_blend_mode(SGP_BLENDMODE_NONE);  // restore
```

---

## Phase 4 — Text rendering (`app/text_renderer.h/.cpp`)

**Branch:** `refactor/sokol-text`  
**Files:** `app/text_renderer.h`, `app/text_renderer.cpp`

The `TextContext` / `text_draw` / `text_draw_number` call sites in
`render_system.cpp` are preserved exactly **except** that the
`SDL_Renderer* renderer` parameter is removed from `text_draw` and
`text_draw_number` — sokol_gp is a global context, no handle needed.
All call sites in `render_system.cpp` must drop that argument.
The implementation changes from SDL2_ttf to stb_truetype + sokol_gp.

### TextContext — new internals

```cpp
struct GlyphInfo {
    stbtt_packedchar data[96];   // ASCII 32–127
};

struct TextContext {
    sg_image       atlas;           // GPU texture, R8 format
    GlyphInfo      glyphs[3];       // one per FontSize (Small, Medium, Large)
    float          font_sizes[3];   // pixel heights per FontSize
    sg_sampler     sampler;         // linear filter for text
    // No TTF_Font*, no SDL_Texture*
};
```

### Initialisation (`text_init`)

```
1. Read .ttf file bytes into a buffer (std::vector<uint8_t>, fread)
2. Allocate a CPU-side atlas bitmap (e.g. 512×512, uint8_t)
3. stbtt_PackBegin → stbtt_PackFontRanges for each FontSize → stbtt_PackEnd
4. sg_make_image with SG_PIXELFORMAT_R8, atlas bitmap as initial data
5. sg_make_sampler with linear filter
```

On Emscripten the font file is loaded from the virtual FS populated by
`--preload-file assets@/assets` in the linker flags.

### Drawing (`text_draw`)

For each character in the string:

```
stbtt_GetPackedQuad → aligned_quad (screen dest rect + atlas UV rect)
sgp_set_image(0, text_ctx.atlas)
sgp_set_sampler(0, text_ctx.sampler)
sgp_draw_textured_rect(0,
    {dest.x, dest.y, dest.w, dest.h},   // screen position
    {uv.x,   uv.y,   uv.w,  uv.h  })   // atlas region
sgp_reset_image(0)
```

`TextAlign::Center` is computed by summing advance widths across the string
before issuing any draw calls (same logic as current implementation).

### Shutdown (`text_shutdown`)

```cpp
sg_destroy_image(ctx.atlas);
sg_destroy_sampler(ctx.sampler);
```

No `TTF_CloseFont`, no `TTF_Quit`.

---

## Phase 5 — Audio (`audio_system`)

**Branch:** `refactor/sokol-audio`  
**Files:** `app/systems/render_system.cpp` (audio stub is compiled into this TU;
  the function declaration is in `app/systems/all_systems.h`)

Currently a no-op stub (`audio_clear` only). When real audio is implemented:

- Replace `Mix_PlayChannel` comments with `saudio_push` for streaming, or
  decode audio clips with `stb_vorbis` / miniaudio at init and push samples
  via the `sokol_audio.h` stream callback.
- `AudioQueue` component and `audio_system` signature are unchanged.
- On Web, sokol_audio uses the Web Audio API via Emscripten. Audio context
  activation is gated by user gesture — the existing `GamePhase::Title`
  tap-to-start screen satisfies this requirement.

This phase is **independent** of phases 1–4 and can be deferred.

---

## Phase 6 — Web CI job (new)

**Branch:** `refactor/sokol-web-ci`  
**Files:** `.github/workflows/ci-web.yml` (new), `app/web/shell.html` (new)

Add a fourth CI job for Emscripten builds:

```yaml
name: CI (Web)
runs-on: ubuntu-latest
steps:
  - uses: actions/checkout@v6
  - uses: mymindstorm/setup-emsdk@v14
    with:
      version: latest
  - name: Build (Emscripten)
    run: |
      emcmake cmake -B build-web -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake
      cmake --build build-web
  - name: Upload artifact
    uses: actions/upload-artifact@v4
    with:
      name: shapeshifter-web
      path: build-web/shapeshifter.*   # .js + .wasm + .html
```

No tests run on the web job (Emscripten guard in CMakeLists.txt skips the
`shapeshifter_tests` target).

---

## Phase 7 — Linux CI cleanup

**Branch:** (fold into `refactor/sokol-main-loop` or a separate cleanup PR)  
**Files:** `.github/workflows/ci-linux.yml`

Remove the `apt-get install` step for SDL2 and SDL2_ttf:

```yaml
# Remove entirely:
- name: Install system dependencies
  run: |
    sudo apt-get install -y libsdl2-dev libsdl2-ttf-dev catch2
```

Linux system `catch2` may still be needed if vcpkg is not used for it on
Linux. Evaluate during implementation.

---

## Files touched — summary

| File | Change |
|---|---|
| `app/external/sokol/` | New — 7 headers + 7 impl TUs |
| `app/external/sokol_gp/` | New — 1 header + 1 impl TU |
| `app/external/stb/` | New — 2 headers + 1 impl TU |
| `app/external/VERSIONS.md` | New — pinned commit hashes |
| `CMakeLists.txt` | Heavy — swap deps, add sokol_impl target, platform backends |
| `vcpkg.json` | Remove sdl2, sdl2-ttf |
| `app/main.cpp` | Structural rewrite — callbacks replace main loop |
| `app/systems/all_systems.h` | Remove SDL_Renderer forward decl, update render_system sig |
| `app/systems/render_system.cpp` | Heavy — every draw call replaced with sgp_* |
| `app/systems/input_system.cpp` | Thin — delete SDL event loop, keep duration tick |
| `app/text_renderer.h` | Swap internals, preserve public API |
| `app/text_renderer.cpp` | Rewrite — SDL2_ttf → stb_truetype + sokol_gp |
| `app/web/shell.html` | New — Emscripten HTML shell |
| `.github/workflows/ci-linux.yml` | Remove SDL system dep install |
| `.github/workflows/ci-windows.yml` | Remove SDL/SDL_ttf references |
| `.github/workflows/ci-web.yml` | New — Emscripten build job |

**Unaffected:** All ECS components, all game-logic systems (collision,
scoring, energy, gesture, etc.), all tests.

---

## Execution order

Each phase should be its own PR, merged in order, keeping `main` green
throughout. Phases 0 and 1 are the gate — nothing else compiles until the
build system and main loop land.

```
Phase 0 — vendor + CMake      → CI must pass on all 3 native platforms
Phase 1 — main loop           → game runs (no rendering yet, black window)
Phase 2 — input               → input works, game logic ticks
Phase 3 — render              → game is visually complete
Phase 4 — text                → HUD and overlays readable
Phase 5 — audio               → (can be deferred post-launch)
Phase 6 — web CI              → web build verified on every PR
Phase 7 — Linux CI cleanup    → remove SDL system deps from CI
```
