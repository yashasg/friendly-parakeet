# EnTT + SDL2 + GLM API Replacement Map

**Date:** 2025-01-14  
**Scope:** Bullet Hell / Shapeshifter codebase  
**Goal:** Enable ruthless cleanup by cataloging direct replacements for runtime_api.h wrappers  

---

## I. Type Aliases & Constants Mapping

### Direct Replacements (Safe, Low Risk)

| Wrapper | Current Source | Replacement | Rationale | Usage Examples |
|---------|----------------|-------------|-----------|-----------------|
| `Vector2` | `SDL_FPoint` | `glm::vec2` | GLM is already a dependency; vec2 is standard 2D vector type | `components/transform.h:9`, `rendering/renderer_backend_sdl2.cpp:140` |
| `Vector3` | Custom struct | `glm::vec3` | GLM is standard for 3D math; struct→vec3 conversion already exists | `runtime_compat.cpp:132-134` (to_glm_vec3) |
| `Vector4` | Custom struct | `glm::vec4` | GLM vec4 for 4D homogeneous coords / RGBA vectors | Not heavily used; safe to introduce |
| `Rectangle` | Custom struct | `glm::vec4` or keep | Rectangle is 2D layout (x,y,w,h), not a vector operation—**KEEP as-is** for semantic clarity | `runtime_api.h:29`, `rendering/renderer_backend_sdl2.cpp:100` |
| `Color` | `SDL_Color` | Keep `SDL_Color` directly | SDL2 native; already aliased as `Color`; no benefit to indirect | `constants.h` (shape colors), `ClearBackground()` calls |
| `Matrix` | Custom 4x4 array | `glm::mat4` | GLM is standard; conversion layer exists (`to_glm_mat4`, `from_glm_mat4`) | `rendering/renderer_backend_sdl2.cpp:193-199` |
| `Camera2D` | Custom struct | Keep as-is + direct GL calls | 2D camera is app-specific (offset/target/zoom); no library standard—keep struct but call GL directly | `runtime_compat.cpp:282-309` (BeginMode2D/EndMode2D) |
| `Camera3D` | Custom struct | Keep as-is + use glm::lookAt/glm::perspective | 3D camera struct is fine; internally use GLM for matrix math | `rendering/renderer_backend_sdl2.cpp:203-205` |
| `DEG2RAD` | `PI / 180.0f` | `glm::radians()` | GLM has built-in radians function; replace const with inline function calls | `rendering/renderer_backend_sdl2.cpp:136` (DEG2RAD) |
| `PI` | `3.14159...` | `glm::pi<float>()` | GLM 0.9.9+ has glm::pi constant | Check GLM version; fallback to macro if needed |

---

## II. Input & Window Functions

### Low Risk (Direct SDL2 Replacements)

| Wrapper | Current | Direct Replacement | Rationale | Callsite |
|---------|---------|-------------------|-----------|----------|
| `GetMousePosition()` | `SDL_GetMouseState()` + scaling | `SDL_GetMouseState()` in input system only; remove scale/offset wrapper | Only `pointer_input.h` reads mouse; centralize in input layer | `runtime_compat.cpp:219-227` |
| `IsMouseButtonDown(button)` | SDL button state check | `SDL_GetMouseState()` + button mask directly | Move to input layer; remove wrapper | `runtime_compat.cpp:233-240` |
| `IsMouseButtonPressed(button)` | SDL button state + frame tracking | `platform::sdl2::input_key_pressed()` (already internal) | Already using platform abstraction; expose directly | `runtime_compat.cpp:242-252` |
| `IsMouseButtonReleased(button)` | SDL button state + frame tracking | Platform input layer | Already implemented; use directly | `runtime_compat.cpp:255-266` |
| `IsKeyDown(key)` | `SDL_GetKeyboardState()` | Direct SDL call | Move to input system; expose scancode directly | `runtime_compat.cpp:268-272` |
| `IsKeyPressed(key)` | `platform::sdl2::input_key_pressed()` | Keep platform layer; use directly | Already abstracted correctly | `runtime_compat.cpp:274-276` |
| `GetCharPressed()` | `platform::sdl2::input_consume_char_pressed()` | Keep platform layer; use directly | Already abstracted; correct abstraction level | `runtime_compat.cpp:278-280` |
| `SetMouseOffset()` / `SetMouseScale()` | Global state variables | Centralize in input context or InputState component | These are screen→virtual coord transforms; belong in rendering layer | `runtime_compat.cpp:209-217` |
| `GetScreenWidth()` / `GetScreenHeight()` | `platform::sdl2::screen_width/height()` | Use platform layer directly (already done in compat) | These are already thin wrappers; expose platform functions directly | `runtime_compat.cpp:335-341` |
| `IsWindowReady()` | `platform::sdl2::is_ready()` | Use platform layer directly | Already thin wrapper | `runtime_compat.cpp:205-207` |

---

## III. Rendering Functions

### Medium Risk (Requires Careful Porting)

| Wrapper | Current | Replacement Strategy | Rationale | Callsite |
|---------|---------|---------------------|-----------|----------|
| `BeginMode2D(Camera2D)` | Direct GL ortho setup | Inline GL calls OR `platform::graphics::` functions | Low-level GL state; move to renderer backend or inline in render system | `runtime_compat.cpp:282-299` |
| `EndMode2D()` | GL pop matrix | Inline GL calls OR `platform::graphics::` | Partner to BeginMode2D | `runtime_compat.cpp:301-309` |
| `ClearBackground(Color)` | `glClearColor()` + `glClear()` | `platform::graphics::clear_background()` (already exists!) | Function already extracted; use directly | `runtime_compat.cpp:311-322` |
| `BeginScissorMode()` / `EndScissorMode()` | GL scissor enable/disable | Direct GL calls OR inline in render system | Low-level GL state; inline directly in rendering pass | `runtime_compat.cpp:324-333` |
| `DrawRectangle()` / `DrawRectangleRec()` etc. | `draw_gl_rect()` helper | Inline GL quads in renderer backend OR use `platform::graphics::` module | Primitive draw calls; use renderer backend module directly | `runtime_compat.cpp:351-392` |
| `DrawLine()` / `DrawLineV()` | GL line primitives | Inline GL calls in game render system | Low-level GL; inline in render layer | `runtime_compat.cpp:394-410` |
| `DrawTriangle()` / `DrawCircleV()` | GL primitives | Inline GL in render system | Low-level GL; inline directly | `runtime_compat.cpp:412-430` |
| `BeginMode3D(Camera3D)` | GLM math + GL projection/view setup | `platform::graphics::begin_mode_3d()` (already extracted!) | Function already extracted to renderer backend; use directly | `rendering/renderer_backend_sdl2.cpp:183-212` |
| `EndMode3D()` | GL state restore | `platform::graphics::end_mode_3d()` (already extracted!) | Already extracted; use directly | `rendering/renderer_backend_sdl2.cpp:214-223` |

**Note:** Many drawing functions are already abstracted into `platform::graphics::*` namespace—prefer using those over runtime_api.h.

---

## IV. Matrix & Math Functions

### Low Risk (Trivial GLM Replacements)

| Wrapper | Current Implementation | Direct Replacement | Callsite | Notes |
|---------|----------------------|-------------------|----------|-------|
| `MatrixIdentity()` | Custom identity | `glm::mat4(1.0f)` | `runtime_compat.cpp:641-643` | Trivial |
| `MatrixMultiply(A, B)` | Custom multiply via glm | `glm::mat4 * glm::mat4` | `runtime_compat.cpp:645-648` | Use GLM operator* directly |
| `MatrixTranslate(x,y,z)` | `glm::translate()` wrapper | `glm::translate(glm::mat4(1.0f), glm::vec3{x,y,z})` | `runtime_compat.cpp:649-652` | Direct GLM call |
| `MatrixScale(x,y,z)` | `glm::scale()` wrapper | `glm::scale(glm::mat4(1.0f), glm::vec3{x,y,z})` | `runtime_compat.cpp:653-656` | Direct GLM call |
| `MatrixRotateY(angle)` | `glm::rotate()` wrapper | `glm::rotate(glm::mat4(1.0f), angle, glm::vec3{0,1,0})` | `runtime_compat.cpp:657-660` | Direct GLM call; use glm::radians(angle) |
| `MatrixOrtho(...)` | `glm::ortho()` wrapper | `glm::ortho(left, right, bottom, top, znear, zfar)` | `runtime_compat.cpp:661-669` | Direct GLM call |
| `MatrixPerspective(...)` | `glm::perspective()` wrapper | `glm::perspective(glm::radians(fovY), aspect, znear, zfar)` | `runtime_compat.cpp:670-676` | Direct GLM call |
| `MatrixLookAt(eye, target, up)` | `glm::lookAt()` wrapper | `glm::lookAt(to_glm_vec3(eye), to_glm_vec3(target), to_glm_vec3(up))` | `runtime_compat.cpp:677-680` | Direct GLM call |
| `MatrixToFloat()` | Returns pointer | `glm::value_ptr(mat)` | `runtime_compat.cpp:681-686` | GLM utility |
| `Vector3Transform(v, mat)` | GLM vec4 multiply | `glm::vec3(to_glm_mat4(mat) * glm::vec4(v, 1.0f))` | `runtime_compat.cpp:687-691` | Direct GLM math |
| `Clamp()`, `Lerp()`, `Fade()` | Simple math | `glm::clamp()`, `glm::mix()`, manual RGBA scaling | `runtime_compat.cpp:692-706` | GLM has glm::clamp, glm::mix for Lerp |
| `GetWorldToScreenEx()` | Complex projection | Inline GLM math (already done in compat) | `runtime_compat.cpp:877-902` | Keep as-is or inline into rendering layer |

**Action:** Replace one-by-one in systems that use these; verify no type mismatches.

---

## V. Font & Text Functions

### Medium Risk (Mostly Internal Implementation)

| Wrapper | Current | Replacement | Rationale | Callsite |
|---------|---------|-----------|-----------|----------|
| `LoadFontEx()` | Custom impl | Keep wrapper; internalize to asset system | Font loading is game-specific; leave abstraction | `runtime_compat.cpp:788-803` |
| `UnloadFont()` | SDL cleanup | Keep wrapper; keep current cleanup logic | Game owns font lifecycle | `runtime_compat.cpp:804-813` |
| `GetFontDefault()` | SDL default font | Keep wrapper | Game-specific default font source | `runtime_compat.cpp:814-821` |
| `MeasureTextEx()` | SDL text size query | Keep wrapper but replace internals with SDL directly | Wrapper is thin; update to call SDL text functions | `runtime_compat.cpp:715-734` |
| `DrawTextEx()` | SDL text rendering | Keep wrapper but use SDL or direct rendering | Wrapper is thin; update to use rendering backend | `runtime_compat.cpp:735-786` |
| `TextFormat()` | `snprintf` wrapper | Direct `snprintf()` calls OR use `std::format()` (C++20) | Trivial; replace inline | `runtime_compat.cpp:822-830` |
| `TextToInteger()` | `std::stoi()` wrapper | Direct `std::stoi()` or `std::atoi()` | Trivial; replace inline | `runtime_compat.cpp:831-835` |
| Codepoint functions | UTF-8 helpers | Keep as-is (internal SDL usage) | Complex Unicode handling; keep centralized | `runtime_compat.cpp:836-858` |
| `ColorToInt()` | RGBA→int packing | Inline bit packing OR keep as utility | Trivial; safe to inline | `runtime_compat.cpp:860-865` |

---

## VI. Collision & Geometry Functions

### Low Risk (Trivial Math)

| Wrapper | Current | Replacement | Callsite |
|---------|---------|-----------|----------|
| `CheckCollisionPointRec(point, rec)` | AABB check | Inline: `point.x >= rec.x && point.x < rect.x+rect.w && ...` | `runtime_compat.cpp:867-870` |
| `CheckCollisionRecs(rec1, rec2)` | AABB check | Inline AABB overlap test | `runtime_compat.cpp:872-875` |

---

## VII. Audio Functions

### High Risk (System-Critical; Keep Abstraction)

| Wrapper | Current | Replacement | Rationale | Callsite |
|---------|---------|-----------|-----------|----------|
| `LoadWave()` | SDL audio file load | Keep wrapper; maintain platform abstraction | Audio platform layer is correct abstraction | `runtime_compat.cpp:903-928` |
| `LoadSoundFromWave()` | Audio handle creation | Keep wrapper | Audio lifecycle managed by system | `runtime_compat.cpp:966-985` |
| `PlaySound()` | Audio playback | Keep wrapper | Keep audio backend abstracted | `runtime_compat.cpp:994-1002` |
| `UnloadSound()` / `UnloadWave()` | Audio cleanup | Keep wrapper | RAII; don't expose SDL details | `runtime_compat.cpp:1003-1011, 929-936` |
| `LoadMusicStream()` | Audio streaming | Keep wrapper | Keep platform abstraction | `runtime_compat.cpp:1012-1025` |
| Music playback functions | SDL music control | Keep wrapper; refactor into audio_system | Audio system owns playback; keep abstracted | `runtime_compat.cpp:1026-1082` |

**Action:** Keep audio layer intact; it's a correct abstraction boundary.

---

## VIII. Logging & Utility Functions

### Low Risk (Trivial or Already Internal)

| Wrapper | Current | Replacement | Action |
|---------|---------|-----------|--------|
| `SetTraceLogLevel()` | Global log level | Keep or inline into logger | `app/util/file_logger.h` already manages logging |
| `SetTraceLogCallback()` | Global callback | Keep or integrate into logger system | File logger is custom abstraction; keep |
| `TraceLog()` | `vfprintf` wrapper | Use file_logger.h directly | Already custom logging; keep using it |
| `FileExists()` | `std::filesystem::exists()` | Direct `std::filesystem::exists()` | Trivial; can inline |
| `GetApplicationDirectory()` | Static path string | Direct `std::filesystem::current_path()` | Trivial; can inline |

---

## IX. Prioritized Replacement Playbook

### Phase 1: Low Risk, High Impact (Do First)
**Risk: LOW | Impact: HIGH**

1. **Vector Types (Vector2→glm::vec2, Vector3→glm::vec3)**
   - Replace in: `components/transform.h`, input layer, rendering layer
   - Files affected: ~20 files
   - Replacement pattern: `Vector2 v; → glm::vec2 v;` + `using Vector2 = glm::vec2;` in runtime_api.h
   - Callsites: `pointer_input.h:X`, `rendering/renderer_backend_sdl2.cpp:140`

2. **Matrix Functions (Use GLM directly)**
   - Replace all `MatrixIdentity()`, `MatrixTranslate()`, etc. with `glm::mat4` operators
   - Files: `rendering/renderer_backend_sdl2.cpp`, any game systems using transforms
   - Pattern: `Matrix m = MatrixTranslate(x,y,z); → glm::mat4 m = glm::translate(glm::mat4(1.0f), {x,y,z});`

3. **Simple Math (Clamp, Lerp, etc.)**
   - Replace with `glm::clamp()`, `glm::mix()`
   - Pattern: `Clamp(x, min, max); → glm::clamp(x, min, max);`

4. **Collision Helpers (CheckCollisionPointRec, CheckCollisionRecs)**
   - Inline AABB checks directly in systems
   - Pattern: Replace call with inline boolean logic

### Phase 2: Medium Risk, Medium Impact (Do Second)
**Risk: MEDIUM | Impact: MEDIUM**

1. **2D/3D Camera Setup (BeginMode2D/EndMode2D → Inline GL)**
   - Move BeginMode2D/EndMode2D GL setup inline into render systems
   - Use `platform::graphics::begin_mode_3d()` which already exists
   - Files: rendering system, HUD render pass

2. **Drawing Primitives (DrawRectangle, DrawLine, etc.) → Use platform::graphics or Inline**
   - Redirect to `platform::graphics::*` namespace functions
   - Pattern: `DrawRectangle(...) → platform::graphics::draw_rectangle(...)`
   - Or inline GL calls directly in renderer backend

3. **Input Wrapper Exposure**
   - Expose `platform::sdl2::input_*` functions directly
   - Remove `GetMousePosition()` wrapper; use `SDL_GetMouseState()` in input layer only
   - Files: `input/` layer, `pointer_input.h`

### Phase 3: High Risk, Low Impact (Keep As-Is OR Refactor Carefully)
**Risk: HIGH | Impact: LOW**

1. **Audio Functions**
   - KEEP as-is; these are correct abstraction boundaries
   - Do NOT replace—audio platform layer is well-designed

2. **Rectangle & Camera3D Structs**
   - KEEP as-is (app-specific semantics)
   - Rectangle: Keep as-is (x, y, w, h layout is semantic)
   - Camera3D: Keep struct, but internally use GLM for math

3. **Font/Text Helpers**
   - KEEP wrappers; refactor internals if needed but keep facade
   - Logging functions: KEEP (already using file_logger.h)

---

## X. Unsafe Replacements (DO NOT DO)

| Item | Why Not | Consequence |
|------|---------|-----------|
| Remove `Rectangle` struct | Semantic clarity loss; AABB ≠ vector | Would need to scatter magic numbers like (x, y, w, h) into callsites |
| Replace audio layer | Breaks platform abstraction | Coupling audio playback to SDL; hard to port to emscripten, iOS, WASM |
| Inline `BeginMode2D()` everywhere | Increases GL state management burden | Render systems become fragile; harder to debug state leaks |
| Replace logger with direct prints | Breaks log level control | Can't control verbosity at runtime |

---

## XI. Implementation Checklist (Per-Phase)

### Phase 1 Checklist
- [ ] Add `using Vector2 = glm::vec2;` to runtime_api.h  
- [ ] Add `using Vector3 = glm::vec3;` to runtime_api.h  
- [ ] Replace all `Vector2` type annotations in headers  
- [ ] Update `MeasureTextEx()` return type  
- [ ] Replace `Matrix` functions with GLM equivalents  
- [ ] Test: Verify all transform calculations match old behavior  
- [ ] Run build with `-Wall -Wextra -Werror`  

### Phase 2 Checklist
- [ ] Extract 2D camera setup into `platform::graphics::begin_mode_2d()` if missing  
- [ ] Redirect `DrawRectangle()` calls to `platform::graphics::*`  
- [ ] Expose `platform::sdl2::input_*` functions  
- [ ] Remove input wrapper layer  
- [ ] Test: Input system still tracks frame-to-frame state correctly  

### Phase 3 Checklist
- [ ] VERIFY audio system tests pass  
- [ ] Confirm Rectangle struct usage in rendering layer  
- [ ] Confirm Camera3D works with GLM math  
- [ ] Final integration test: Run full game, no crashes  

---

## XII. Concrete Callsite Examples (For Cleanup Loops)

### Example 1: Vector2 Replacement
**Before:**
```cpp
// runtime_api.h
using Vector2 = SDL_FPoint;

// components/transform.h
struct WorldTransform {
    Vector2 position = {0.0f, 0.0f};
    ...
};
```

**After:**
```cpp
// runtime_api.h
using Vector2 = glm::vec2;

// components/transform.h (no change needed)
struct WorldTransform {
    Vector2 position = {0.0f, 0.0f};  // now glm::vec2
    ...
};
```

---

### Example 2: Matrix Function Replacement
**Before:**
```cpp
// rendering/renderer_backend_sdl2.cpp
Matrix projection = MatrixPerspective(camera.fovy, aspect, 0.01f, 10000.0f);
glm::mat4 glm_proj = to_glm_mat4(projection);  // unnecessary conversion
```

**After:**
```cpp
// rendering/renderer_backend_sdl2.cpp
glm::mat4 projection = glm::perspective(glm::radians(camera.fovy), aspect, 0.01f, 10000.0f);
```

---

### Example 3: Input Wrapper Removal
**Before:**
```cpp
// game_render_system or input system
Vector2 mouse_pos = GetMousePosition();
```

**After:**
```cpp
// Directly in input layer or input_system.cpp
int x = 0, y = 0;
SDL_GetMouseState(&x, &y);
Vector2 mouse_pos{static_cast<float>(x), static_cast<float>(y)};
// Apply screen transform here if needed
```

---

### Example 4: Drawing Primitive Redirection
**Before:**
```cpp
DrawRectangle(10, 20, 100, 50, RED);
```

**After:**
```cpp
// Option A: Use platform graphics backend (if function exists)
platform::graphics::draw_rectangle_filled({10.f, 20.f, 100.f, 50.f}, RED);

// Option B: Inline GL (if no platform function)
glColor4ub(RED.r, RED.g, RED.b, RED.a);
glBegin(GL_QUADS);
glVertex2f(10.f, 20.f);
glVertex2f(110.f, 20.f);
glVertex2f(110.f, 70.f);
glVertex2f(10.f, 70.f);
glEnd();
```

---

## XIII. Build Verification

After each phase, run:
```bash
cmake -B build -S . -DCMAKE_CXX_FLAGS="-Wall -Wextra -Werror"
cmake --build build
./build/shapeshifter_tests  # Verify all tests pass
./build/shapeshifter        # Quick manual smoke test
```

---

## XIV. References

- **GLM Docs:** https://glm.g-truc.net/ (already included in vcpkg.json)
- **SDL2 Reference:** https://wiki.libsdl.org/
- **EnTT Guide:** https://github.com/skypjack/entt/wiki
- **Raylib (for comparison):** https://www.raylib.com/ (our wrappers are raylib-compatible)

---

**End of API Replacement Map**
