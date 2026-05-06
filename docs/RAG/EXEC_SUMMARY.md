# LVGL Migration Map — Executive Summary

## Artifact Location
📄 **rag-lvgl-map.md** (514 lines)

## Key Findings

### Current State
- **Text Layer:** SDL2_ttf + raylib `text_draw()` (27 call sites)
- **UI System:** `ui_render_system()` — phase-based 2D rendering
- **Input Routing:** Two-tier entt dispatcher (raw → semantic events)
- **Build:** LVGL XML→C codegen partial; not integrated into runtime

### Migration Scope
| Layer | Current | Target | Effort |
|-------|---------|--------|--------|
| Text | SDL2_ttf | LVGL labels | 5-7 days (Phase 1-3) |
| UI | Raylib 2D | LVGL screens | 10-12 days (Phase 2) |
| Input | raylib API | LVGL input devices | 3-5 days (Phase 1) |
| Build | Partial CMake | Full integration | 2-3 days (Phase 0) |

### Phased Execution (5 Phases)

**Phase 0: Validation & Setup** (2-3 days)
- Wire LVGL runtime init/timer
- Verify codegen + link
- Create adapter stubs

**Phase 1: Runtime Wiring** (3-5 days)
- Hook LVGL input devices
- Load fonts via `_lved_project_init()`
- Verify semantic events flow

**Phase 2: Screen Migration** (10-12 days)
- Migrate: Title → LevelSelect → Playing → GameOver (each 2-3 days)
- Replace `text_draw()` → `lv_label_set_text_fmt()` per phase
- Test: input, transitions, HUD updates

**Phase 3: Cleanup** (2-3 days)
- Remove all `text_draw()` calls
- Delete `TextContext` / `text_renderer.{h,cpp}`
- Remove SDL2_ttf from CMake

**Phase 4: Polish & Testing** (2-3 days)
- Visual regression validation
- Cross-platform testing (Desktop/Web/Mobile)
- Documentation

### Critical Dependencies (Cannot Remove Before)

| Module | Blocker | Guard |
|--------|---------|-------|
| SDL2_ttf | All 27 `text_draw()` calls | Zero text_draw calls in codebase |
| `TextContext` | Fallback font refs | LVGL font loading verified |
| `BeginMode2D/EndMode2D` | UI overlays | All overlays → LVGL objects |
| `gesture_input` | Swipe detection | LVGL gesture adapter wired |

### Regression Prevention

**Before Phase 1:**
- ✓ LVGL codegen compiles
- ✓ `_lved_project_init()` loads fonts
- ✓ LVGL timer runs without deadlock
- ✓ Input devices wired

**During Phase 2:**
- ✓ Screen transitions work
- ✓ Semantic events fire
- ✓ Input routes correctly
- ✓ No resource leaks

**After Phase 3:**
- ✓ Zero warnings (`-Wall -Wextra -Werror`)
- ✓ Full game loop: Title → Play → GameOver
- ✓ All HUD visible & correct
- ✓ Cross-platform verified

### Concrete Migration Map

**Text Rendering (27 sites):**
```
text_draw() + text_init() + text_shutdown()
  ↓
lv_label_create() + _lved_project_init() + (LVGL auto-cleanup)
```

**Input Routing (entt dispatcher):**
```
raylib input → InputEvent → gesture_routing → GoEvent/ButtonPressEvent
  ↓
LVGL indev device → adapter callback → GoEvent/ButtonPressEvent (same)
  (Semantic event queue unchanged; only source swapped)
```

**UI Rendering:**
```
ui_render_system() + BeginMode2D() + text_draw()
  ↓
phase transitions → lv_scr_load_anim() + lv_label_set_text_fmt()
  (No explicit 2D mode; LVGL renders via lv_timer_handler())
```

### Build Integration (CMake)

**Current:**
- ✗ LVGL XML→C codegen exists but not linked
- ✗ Generated files not in build targets

**After Phase 0:**
- ✓ `lvgl_ui_codegen` target runs automatically
- ✓ Generated .c/.h linked into executable
- ✓ `_lved_project_init()` available to game code

### Testing Matrix

| Platform | Phase | Text | Input | HUD Updates |
|----------|-------|------|-------|-------------|
| Desktop | Title | [ ] | [ ] | N/A |
| Desktop | Playing | [ ] | [ ] | [ ] |
| Web | Title | [ ] | [ ] | N/A |
| Web | Playing | [ ] | [ ] | [ ] |
| iOS | Title | [ ] | [ ] | N/A |
| iOS | Playing | [ ] | [ ] | [ ] |

---

## Recommended Execution Order

1. **Week 1:** Phase 0 (setup) + Phase 1 (runtime)
2. **Week 2-3:** Phase 2 (screen migration)
3. **Week 3:** Phase 3 (cleanup) + Phase 4 (testing)

## Artifacts

✅ **rag-lvgl-map.md** — Full mapping (8 sections, 514 lines)
  - Current stack analysis
  - LVGL API equivalents (4 mapping tables)
  - Dependency guards & regression tests
  - Phased checklist with gates
  - Concrete code samples

## Next Steps

1. Review **rag-lvgl-map.md** (reference copy)
2. Approve phase schedule in next cleanup loop
3. Begin Phase 0: LVGL runtime wiring
4. Iterate Phase 1-4 per checklist

---

**Status:** Planning Complete | No production code modified
**Date:** 2025-05-05 | **Scope:** Cleanup loop preparation
