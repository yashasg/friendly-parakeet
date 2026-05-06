# LVGL Migration Map & Cleanup Loop Guide

**Project:** SHAPESHIFTER (Bullet Hell Game)
**Target:** C++17 + LVGL for UI/Text/Input replacement
**Scope:** Text rendering, UI components, input routing
**Status:** Planning phase (no code modifications)

---

## 1. CURRENT UI/TEXT/INPUT STACK ANALYSIS

### 1.1 Text Rendering Layer (`app/ui/text_renderer.*`)

**Current Implementation:**
- SDL2_ttf backend: loads `.ttf` font at 3 sizes (Small=16pt, Medium=28pt, Large=48pt)
- Pre-loaded fonts stored in `TextContext` (ECS registry context singleton)
- API: `text_draw()`, `text_draw_number()`, `text_width()` (query)
- Alignment: `TextAlign::Left/Center/Right`
- Colors: RGBA uint8_t tuple
- Drawing: Direct raylib calls (`DrawTextEx()`) within `ui_render_system`

**Invocation Chain:**
1. `game_loop_init()` → `text_init_default(text_ctx)` / `text_init(text_ctx, path)`
2. `ui_render_system()` queries `TextContext` from registry
3. `text_draw()` delegates to raylib `DrawTextEx()` with SDL2_ttf `Font`
4. `game_loop_shutdown()` → `text_shutdown(text_ctx)`

**Usage Density:** 27 call sites (text_draw, text_draw_number, text_width, init, shutdown)

### 1.2 UI Components & Rendering (`app/systems/ui_render_system.cpp`)

**Current Implementation:**
- Raylib 2D mode: `BeginMode2D()` / `EndMode2D()` wrapping all UI draws
- Screen-space overlays (HUD, banners, popups)
- Phase-based rendering logic (Title, LevelSelect, Playing, Paused, GameOver, SongComplete, Settings, Tutorial)
- Geometric overlays: `DrawRectangleRec()` for pause dimming (semi-transparent black)
- Dynamic text assembly: score, energy, beat counter formatted into char buffers

**Components Rendered:**
- **PopupDisplay**: floating score indicators, timing feedback (via `PopupDisplay` ECS component)
- **ScreenPosition**: anchors for popup placement
- **GamePhase banners**: title banners with prompts (centered text)
- **HUD elements**: score/chain, energy %, beat count

**Dependency Graph:**
```
ui_render_system()
  ├─ TextContext (registry ctx)
  ├─ GameState (read phase + timer)
  ├─ PopupDisplay entities (HUD layer)
  ├─ ScreenPosition entities
  ├─ ScoreState (score, chain display)
  ├─ EnergyState (energy % display)
  ├─ SongState (beat counter)
  ├─ LevelSelectState (selected level/difficulty)
  ├─ GameOverState (death reason)
  └─ raylib 2D drawing API
```

### 1.3 Input Routing & Event Dispatch (`app/input/*`)

**Current Routing Topology:**

```
Raw Input Events
  ↓ [InputState in registry]
  ↓
InputEvent (entt dispatcher, Tier-1 pre-fixed-step)
  ↓ gesture_routing_handle_input()
  ├─ Converts swipes → GoEvent
  └─ Converts button presses → ButtonPressEvent
  ↓ [Tier-2 fixed-step delivery]
  ↓
GoEvent + ButtonPressEvent (entt dispatcher, fixed-step)
  ├─ game_state_handle_go() / game_state_handle_press()
  ├─ level_select_handle_go() / level_select_handle_press()
  └─ player_input_handle_go() / player_input_handle_press()
```

**Key Input Listeners (wired via `wire_input_dispatcher()`):**
- Tier-2 (semantic) listeners registered in order; order = processing priority
- `InputDispatcherWiringState` tracks wiring state (prevents re-wire)

**Input Event Sources:**
- `InputState` (registry ctx): `touch_down`, `touch_up`, `click` flags
- `gesture_input.h`: gesture detection (swipe velocity, angle)
- Enqueued/drained per frame via `InputEvent` dispatcher

### 1.4 CMake Build Integration

**Current LVGL Setup (Partial):**
- XML screen definitions: `content/ui/screens/*.xml` (e.g., `title.xml`)
- LVGL XML→C code generation pipeline:
  - CLI tool detection: `lvgl-cli` or fallback to `Node.js + lved-cli.js`
  - Generated code: `build/generated/lvgl/_lved_project*.{c,h}`
  - Resource zip: `tools/lvgl/lvgl-resources.zip`
- Generated files not yet integrated into build targets
- No LVGL runtime wiring in game_loop

**Current State:**
```cmake
set(LVGL_XML_INPUT_DIR "${CMAKE_SOURCE_DIR}/content/ui/screens")
set(LVGL_XML_OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated/lvgl")
# Detection logic for lvgl CLI (present but not used)
# Code generation custom command exists (lvgl_ui_codegen target)
```

---

## 2. LVGL API MAPPING TABLE

### 2.1 Text Rendering Migration

| **Current (SDL2_ttf+raylib)** | **LVGL Equivalent** | **Migration Notes** |
|---|---|---|
| `TextContext` (Font array) | `lv_font_t*` refs + `_lved_project_init()` | LVGL auto-loads fonts from resources; no manual Font struct |
| `text_init(ctx, path)` / `text_init_default(ctx)` | `_lved_project_init(asset_path)` | One-time init; asset_path = content dir containing fonts |
| `text_draw(ctx, text, x, y, size, r,g,b,a, align)` | `lv_label_create(parent)` + property setters | Create label widget; set text, position, font, color, alignment |
| `text_draw_number(ctx, num, ...)` | `lv_label_create()` + `snprintf()` → `lv_label_set_text()` | Format number → string, set on label |
| `text_width(ctx, text, size)` | `lv_obj_get_width(label)` (post-layout) | Must create transient label, read width, then delete |
| `text_shutdown(ctx)` | No-op (LVGL cleanup via `lv_deinit()` in main loop) | LVGL lifetime = app lifetime |
| `FontSize` enum (Small/Medium/Large) | Font style IDs from `_lved_project.h` | e.g., `&lv_font_montserrat_28` (replace enum with style refs) |
| `TextAlign` enum (Left/Center/Right) | `lv_align_t` (`LV_ALIGN_LEFT/CENTER/RIGHT`) | Direct enum mapping |
| raylib `DrawTextEx()` | Label widget render (automatic) | No explicit draw call; LVGL renders on `lv_timer_handler()` |

### 2.2 UI Components & Screen Management

| **Current (Raylib 2D mode)** | **LVGL Equivalent** | **Migration Notes** |
|---|---|---|
| `BeginMode2D()` / `EndMode2D()` | `lv_scr_act()` context | LVGL screens = implicit; no explicit mode entry/exit |
| `DrawRectangleRec()` overlay | `lv_obj_set_opa_scale()` on screen or modal layer | Opacity blending; or dedicated overlay object |
| Phase-based conditional renders | Screen navigation + event callbacks | Each phase = LVGL screen; `lv_scr_load_anim(screen, anim)` |
| `PopupDisplay` entities + text | Floating label widgets with lifespan system | Create label in `add_popup_display()` system; delete on timeout |
| Dynamic HUD (score, energy, beat) | `lv_label_set_text_fmt()` on persistent labels | Labels created at phase entry; updated via system callbacks |
| Screen-relative positioning (20.0f, 20.0f) | LVGL alignment + offset (`lv_obj_align()` + x/y offset) | E.g., `lv_obj_align(label, LV_ALIGN_TOP_LEFT, 20, 20)` |

### 2.3 Input Event Routing

| **Current (entt dispatcher)** | **LVGL Equivalent** | **Migration Notes** |
|---|---|---|
| `InputEvent` raw queue | `lv_indev_t*` event handlers | LVGL input devices forward touch/key events; hook via callbacks |
| `gesture_routing_handle_input()` | LVGL gesture system + custom callbacks | LVGL has swipe/long-press; custom handler reads `lv_indev_get_gesture_dir()` |
| `GoEvent` / `ButtonPressEvent` semantic queue | Widget event callbacks (`lv_obj_add_event_cb()`) | Buttons emit `LV_EVENT_CLICKED`; custom objects emit custom events |
| `wire_input_dispatcher()` | Set up LVGL input device + register callbacks | `lv_indev_set_read_cb()` → native handler that queues semantic events |
| `InputDispatcherWiringState` | No equiv (LVGL devices are global) | Wiring is single-setup; no re-wire needed |
| Tier-2 semantic handlers | ECS systems dispatch on semantic events | Keep semantic event queue; LVGL input just feeds it |
| `clear_input_events()` | `lv_indev_reset()` (automatic per cycle) | LVGL input state auto-clears; no manual flush |

### 2.4 Generated Code Integration

| **Component** | **LVGL-Generated** | **Integration Point** |
|---|---|---|
| Screen definitions | `_lved_project_gen.h` screen structs (e.g., `scr_title_create()`) | Called from phase handlers |
| Screen widgets (labels, buttons) | Auto-created in `scr_*_create()` functions | Widget tree accessible via generated IDs |
| Event bindings | Custom event callbacks in generated code | Hook game handlers into `lv_obj_add_event_cb()` |
| Font references | `&lv_font_*` in `_lved_project_gen.h` | Replace `FontSize` enum with font refs |
| Resource loading | `_lved_project_init()` | Called once at game_loop_init |

---

## 3. DEPENDENCY & REMOVAL GUARDS

### 3.1 Hard Dependencies (Cannot Remove Before LVGL Runtime Wiring)

| **Module** | **Reason** | **Guard** | **Notes** |
|---|---|---|---|
| `TextContext` struct (components/text.h) | Used in registry ctx for all phases | Still needed until `lv_label_create()` replaces `text_draw()` | Keep as long as any `text_draw()` call exists |
| `text_renderer.{h,cpp}` | All HUD/banner/popup text flows through it | Conditional compilation: `#ifdef SHAPESHIFTER_LVGL_UI` | Can be stubbed or #ifdef'd |
| `ui_render_system()` raylib calls | Phase banners, HUD text, pause overlay | Phase system must route to LVGL screens first | Replace `ui_render_system()` with phase screen loader |
| `gesture_input.h` | Gesture detection (swipe → GoEvent) | LVGL gesture detector + adapter | Can coexist; migrate piecemeal |
| `input_dispatcher` (semantic layer) | Game state machine expects GoEvent/ButtonPressEvent | Keep event queue; swap source (raylib→LVGL) | Semantic layer = isolated; safe to keep |

### 3.2 Safe to Remove Only After Conditions Met

| **Module** | **Removal Condition** | **Migration Step** | **Verification** |
|---|---|---|---|
| SDL2_ttf linkage | All `text_draw()` calls → `lv_label_*()` | Phase 3: Text→Labels | `grep -r "text_draw"` returns 0 |
| `ui_render_system()` | Phase rendering → LVGL screen navigation | Phase 3: Screen routing | No raylib 2D mode in ui_render_system |
| `TextContext` in registry | LVGL font refs cached locally or in phases | Phase 3: Font management | `#ifdef SHAPESHIFTER_LVGL_UI` wraps removal |
| BeginMode2D/EndMode2D in ui layer | All overlays → LVGL screen/layer objects | Phase 3: Overlay replacement | `grep -r "BeginMode2D.*ui"` returns 0 |
| `text_init_default()` | `_lved_project_init()` called at app startup | Phase 1: Init wiring | Game starts without crash + text renders |
| Manual font loading | `_lved_project_init()` handles all fonts | Phase 1: Resource wiring | Font glyphs render in all sizes |

### 3.3 Regression Guards (Pre-Migration Checklist)

**Before Phase 1 begins:**
- [ ] Verify LVGL XML→C generation passes (`.xml` → generated code compiles)
- [ ] Verify generated `_lved_project.h/c` link without errors
- [ ] Verify `_lved_project_init()` loads fonts from asset path
- [ ] Verify LVGL timer handler runs in game loop (no deadlocks)
- [ ] Verify input devices wired to LVGL (touch/keyboard functional)

**During Phase 2 (Screen-by-screen):**
- [ ] Each screen created in LVGL renders before fallback to raylib
- [ ] Semantic events still fire (game state transitions work)
- [ ] Input routes to both LVGL widgets + semantic event queue
- [ ] No texture/resource leaks after phase transitions

**After Phase 3 (Cleanup):**
- [ ] All 27 `text_draw()` sites replaced or stubbed
- [ ] Build passes with `-Wall -Wextra -Werror` (zero warnings)
- [ ] No raylib 2D calls in UI layer
- [ ] SDL2_ttf can be removed from `find_package()` and `target_link_libraries()`
- [ ] Game runs from Title → LevelSelect → Playing → GameOver → loop
- [ ] All HUD elements (score, energy, beat, popups) visible and correct

---

## 4. MAPPING TABLE: CODE LOCATIONS TO LVGL EQUIVALENTS

### 4.1 Text Rendering Sites (27 total)

```
app/systems/ui_render_system.cpp:26  → draw_phase_banner() → lv_label_set_text() x2
app/systems/ui_render_system.cpp:49  → PopupDisplay text → lv_label_create() for popup
app/systems/ui_render_system.cpp:68  → Title banner → lv_scr_load_anim(scr_title)
app/systems/ui_render_system.cpp:72  → Level select banner → lv_scr_load_anim(scr_level_select)
app/systems/ui_render_system.cpp:78  → Level/difficulty → lv_label_set_text_fmt() on level label
app/systems/ui_render_system.cpp:86  → Score/chain text → lv_label_set_text_fmt() on score label
app/systems/ui_render_system.cpp:95  → Energy % text → lv_label_set_text_fmt() on energy label
app/systems/ui_render_system.cpp:100 → Beat count text → lv_label_set_text_fmt() on beat label
app/systems/ui_render_system.cpp:122 → Death reason → lv_label_set_text() on reason label
app/systems/ui_render_system.cpp:131 → Settings banner → lv_scr_load_anim(scr_settings)
... + text_width() calls (measure → transient label width query)
... + text_draw_number() calls (format → set_text_fmt())
... + 15+ additional text_draw() calls in HUD/popup rendering
```

### 4.2 Input Routing Sites

```
app/input/input_dispatcher.cpp:55   → gesture_routing_handle_input wired → 
                                      LVGL indev callback adapter
app/input/input_dispatcher.cpp:60-65 → Semantic handlers wired (keep; feed from LVGL)
app/input/gesture_routing.cpp       → Swipe detection logic →
                                      Port to LVGL gesture + custom callback
app/input/game_state_routing.cpp    → Phase transitions (keep; receive events from LVGL)
```

### 4.3 CMake Integration Sites

```
CMakeLists.txt:115-175   → SDL2_ttf find_package() + target_link_libraries()
                            → Remove after Phase 3 (conditional: #ifdef SHAPESHIFTER_LVGL_UI)
CMakeLists.txt:lvgl_ui_codegen target → Integrate into main build
                                        → Add to app executable dependencies
CMakeLists.txt:UI_SOURCES list         → Add generated LVGL .c files
```

---

## 5. PHASED CLEANUP LOOP CHECKLIST

### Phase 0: Validation & Setup (Gating)

- [ ] **0.1** Verify LVGL XML→C codegen works: `cmake --build build -- lvgl_ui_codegen` succeeds
- [ ] **0.2** Verify generated files exist: `ls build/generated/lvgl/_lved_project*.{c,h}`
- [ ] **0.3** Link generated files into test binary; verify `_lved_project_init()` compiles & links
- [ ] **0.4** Create wrapper: `lv_display_t* lvgl_init_display()` + `lvgl_pump_timer()`
- [ ] **0.5** Create integration stub in game_loop: call `_lved_project_init()` + `lvgl_pump_timer()` in main loop
- [ ] **0.6** Verify app starts & renders without errors (UI may not be visible yet)

**Exit Criteria:** LVGL runtime initialized; timer pumps; no crashes or linker errors.

### Phase 1: Runtime Wiring (Input + Init)

- [ ] **1.1** Wire LVGL input device: `lv_indev_t* indev = lv_indev_create(..., input_read_cb)`
- [ ] **1.2** Create input_read_cb: reads native input state → fills LVGL indev data
- [ ] **1.3** Create gesture adapter: LVGL gesture detector → enqueue GoEvent/ButtonPressEvent
- [ ] **1.4** Call `_lved_project_init(asset_path)` in game_loop_init()
- [ ] **1.5** Verify fonts load: create test label, set font from generated refs, render

**Exit Criteria:** Touch input works in LVGL; fonts render; semantic events flow.

### Phase 2: Screen-by-Screen Migration

**For each phase (Title, LevelSelect, Playing, Paused, GameOver, SongComplete, Settings, Tutorial):**

- [ ] **2.X.1** Create `lvgl_load_phase_<name>()` function that calls `scr_<name>_load_anim()`
- [ ] **2.X.2** Hook game_state transitions to call `lvgl_load_phase_<name>()`
- [ ] **2.X.3** Replace phase-specific `text_draw()` calls with `lv_label_set_text_fmt()` on pre-created widgets
- [ ] **2.X.4** Test: phase transitions; input routes to widgets; HUD text updates
- [ ] **2.X.5** Remove phase's `text_draw()` sites from `ui_render_system()`

**Iterate for:** Title → LevelSelect → Playing → GameOver → (others optional first pass)

**Exit Criteria:** 3+ major phases render via LVGL; semantics preserved; no raylib 2D calls in ui_render_system.

### Phase 3: Cleanup & Removal

- [ ] **3.1** Remove all `text_draw()` / `text_draw_number()` calls from codebase
- [ ] **3.2** Remove `ui_render_system()` raylib rendering logic (keep game loop call stub or remove entirely)
- [ ] **3.3** Remove `TextContext` from game_loop_init() / game_loop_shutdown()
- [ ] **3.4** Stub or remove `text_init_default()` / `text_init()` / `text_shutdown()`
- [ ] **3.5** Comment-out or remove SDL2_ttf `find_package()` and `target_link_libraries()` in CMakeLists.txt

**Exit Criteria:** 
- Zero calls to text_renderer functions remain
- Build passes with `-Wall -Wextra -Werror`
- All HUD/banner/popup text renders via LVGL
- Game loop complete: Title → Play → GameOver → Retry loop works

### Phase 4: Polish & Testing

- [ ] **4.1** Verify all game phases render correctly (visual pass)
- [ ] **4.2** Test input: buttons, swipes, keyboard (full input matrix)
- [ ] **4.3** Test on web (Emscripten): LVGL fonts load, input works
- [ ] **4.4** Test on iOS/Android if applicable
- [ ] **4.5** Update design-docs/architecture.md with LVGL rendering section
- [ ] **4.6** Archive old text_renderer.h/cpp in docs/deprecated/ (reference for future)

---

## 6. CONCRETE MIGRATION GUARDS & REGRESSION TESTS

### 6.1 Unit Tests (Suggested)

```cpp
// tests/test_lvgl_integration.cpp

TEST_CASE("LVGL renders title screen text") {
    auto disp = lvgl_init_display();
    lv_scr_load_anim(scr_title_load());
    lv_timer_handler();  // trigger render
    
    // Verify label exists and text is correct
    auto label = lv_obj_get_child(lv_scr_act(), 0);
    REQUIRE(strcmp(lv_label_get_text(label), "SHAPESHIFTER") == 0);
}

TEST_CASE("Semantic events flow from LVGL input") {
    entt::dispatcher disp;
    bool got_event = false;
    disp.sink<GoEvent>().connect([&](const GoEvent&) { got_event = true; });
    
    // Simulate LVGL button click → enqueue GoEvent
    lvgl_button_clicked_adapter();
    disp.update<GoEvent>();
    
    REQUIRE(got_event);
}
```

### 6.2 Visual Regression Tests

**Before/After Captures:**
- Title screen: banner text + prompt centered
- Level select: banner + level/difficulty info
- Playing: score, chain, energy, beat count in corners
- Popups: floating score indicators
- Overlays: pause dimming

**Manual Test Matrix:**
```
Platform   | Phase       | Text Visible? | Input Works? | Artifacts?
-----------|-------------|---------------|--------------|----------
Desktop    | Title       | [ ]           | [ ]          | [ ]
Desktop    | LevelSelect | [ ]           | [ ]          | [ ]
Desktop    | Playing     | [ ]           | [ ]          | [ ]
Desktop    | GameOver    | [ ]           | [ ]          | [ ]
Web        | Title       | [ ]           | [ ]          | [ ]
Web        | Playing     | [ ]           | [ ]          | [ ]
iOS        | Title       | [ ]           | [ ]          | [ ]
iOS        | Playing     | [ ]           | [ ]          | [ ]
```

### 6.3 Integration Checkpoints

| **Checkpoint** | **Command** | **Expected Output** | **Failure Mode** |
|---|---|---|---|
| LVGL codegen | `cmake --build build -- lvgl_ui_codegen` | Generated .c/.h files in build/generated/lvgl | Code not generated; XML invalid |
| Zero warnings | `cmake --build build 2>&1 \| grep -i warning` | Empty output | Unused vars, implicit casts in text_renderer stubs |
| Text render test | `./build/shapeshifter` (Title screen visible for 2s) | LVGL labels render; no crash | LVGL fonts not loaded; deadlock |
| Input test | Launch game, press any button | Navigates screen (e.g., Title → LevelSelect) | Input device not wired; events not dequeued |
| Semantic events | Enable event logging; press button | `GoEvent/ButtonPressEvent enqueued` | Events not routed from LVGL → game |

---

## 7. IMPLEMENTATION ARTIFACTS & REFERENCE

### 7.1 Generated LVGL Headers to Reference

```
build/generated/lvgl/_lved_project.h      # Public API: _lved_project_init()
build/generated/lvgl/_lved_project_gen.h  # Auto-generated screen/widget functions
build/generated/lvgl/_lved_project_gen.c  # Auto-generated widget creation code
```

**Key Functions from Generated Code:**
- `scr_title_create()` → returns `lv_obj_t*` (screen object)
- `scr_level_select_create()` → returns screen
- Widget getters: `ui_comp_get_<widget_id>()` → access labels/buttons by name

### 7.2 API Adapter Stubs to Create

```cpp
// app/ui/lvgl_adapter.h (NEW)
#pragma once
#include <lvgl.h>

// Init/shutdown
lv_display_t* lvgl_init_display();
void lvgl_shutdown_display();

// Phase routing
void lvgl_load_title_screen();
void lvgl_load_level_select_screen();
void lvgl_load_playing_screen();
// ... etc

// Input wiring
void lvgl_wire_input_devices(entt::registry& reg);

// Utility
void lvgl_set_label_text(lv_obj_t* label, const char* text);
void lvgl_set_label_text_fmt(lv_obj_t* label, const char* fmt, ...);
void lvgl_pump_timer();  // Call in main loop

// Cleanup helpers
void lvgl_cleanup_popups(entt::registry& reg);
```

### 7.3 Modified CMakeLists.txt Sections

**Phase 0:**
```cmake
# Find/link LVGL (already in vcpkg.json?)
find_package(LVGL CONFIG REQUIRED)

# Add generated .c files to build
add_library(lvgl_generated
    build/generated/lvgl/_lved_project_gen.c
    build/generated/lvgl/_lved_project.c
)
target_link_libraries(shapeshifter_lib PUBLIC lvgl_generated LVGL::LVGL)

# Conditionally keep SDL2_ttf until Phase 3
if(NOT SHAPESHIFTER_LVGL_UI)
    find_package(SDL2_ttf CONFIG REQUIRED)
    target_link_libraries(shapeshifter_lib PUBLIC SDL2_ttf::SDL2_ttf)
endif()
```

### 7.4 Inline Adaptation Example

**Current (to be migrated):**
```cpp
void ui_render_system(entt::registry& reg, float /*alpha*/) {
    auto& text_ctx = reg.ctx().get<TextContext>();
    // ... raylib 2D draws + text_draw() calls
    text_draw(text_ctx, "SCORE 100", 20.0f, 20.0f, FontSize::Small, 255, 255, 255, 255);
}
```

**After Phase 3:**
```cpp
void ui_render_system(entt::registry& reg, float /*alpha*/) {
    // LVGL rendering happens automatically in lv_timer_handler()
    // called from main loop. No explicit rendering here.
    lvgl_update_hud_text(reg);  // Update dynamic content only
}

void lvgl_update_hud_text(entt::registry& reg) {
    auto score_label = ui_comp_get_score_label();  // From generated code
    auto& score = reg.ctx().get<ScoreState>();
    lv_label_set_text_fmt(score_label, "SCORE %d", score.displayed_score);
}
```

---

## 8. SUMMARY & NEXT STEPS

### Current State
- **UI Stack:** SDL2_ttf + raylib 2D + custom text_renderer
- **Input:** entt dispatcher with two-tier event model
- **Build:** LVGL XML→C pipeline partially configured; not integrated

### Migration Strategy
1. **Phase 0:** Wire LVGL runtime (init, input device, timer)
2. **Phase 1:** Connect input + font loading
3. **Phase 2:** Migrate screens one-by-one (Title → LevelSelect → Playing → ...)
4. **Phase 3:** Remove SDL2_ttf + text_renderer, clean up stubs
5. **Phase 4:** Visual validation + documentation

### Key Insight
- **Keep semantic event queue** (GoEvent/ButtonPressEvent); only swap source (raylib → LVGL)
- **Phased migration** allows running both parallel (hybrid rendering) during transition
- **No game logic changes** — all changes isolated to UI/input layer

### Artifacts for Next Loop
1. **rag-lvgl-map.md** (this document) — RAG reference for code changes
2. **Test matrix** (Section 6.2) — Regression validation
3. **Phase checklist** (Section 5) — Execution order + gates
4. **CMake snippets** (Section 7.3) — Build integration

---

## Appendix A: File Inventory

| **File** | **Lines** | **Purpose** | **Removal Phase** |
|---|---|---|---|
| app/ui/text_renderer.h | 38 | API definition | Phase 3 (after all text_draw calls removed) |
| app/ui/text_renderer.cpp | 150+ | SDL2_ttf + raylib integration | Phase 3 |
| app/systems/ui_render_system.cpp | 142 | Phase-specific HUD rendering | Phase 2-3 (refactor + remove raylib calls) |
| app/components/text.h | 23 | TextContext struct | Phase 3 (optional if no other uses) |
| app/input/input_dispatcher.cpp | 85 | Event wiring (KEEP) | None (refactor to LVGL source) |
| app/input/gesture_routing.cpp | ~80 | Swipe detection (KEEP/PORT) | None (integrate into LVGL adapter) |
| CMakeLists.txt | ~400 | Build config | Phase 3 (remove SDL2_ttf section) |

---

**Generated:** 2025-05-05 | **Scope:** LVGL Migration Planning (No Code Changes)  
**Next Action:** Approve phase schedule → begin Phase 0 in next cleanup loop
