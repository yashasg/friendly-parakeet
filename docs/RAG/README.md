# LVGL Migration Cleanup Loop — Reference Artifacts

**Project:** SHAPESHIFTER Bullet Hell Game  
**Date:** 2025-05-05  
**Status:** Planning Complete — Ready for Phase 0 Execution

---

## 📚 Artifact Index

### 1. **rag-lvgl-map.md** (24 KB, 514 lines)
   **Comprehensive LVGL Migration Reference**
   
   Sections:
   - §1: Current UI/Text/Input stack analysis (architecture, usages, dependencies)
   - §2: LVGL API mapping tables (4 detailed tables: text, UI, input, codegen)
   - §3: Dependency guards & removal constraints (hard blockers, safe removal criteria)
   - §4: Code location mapping (27 text_render sites, input routes, CMake points)
   - §5: **Phased cleanup checklist** (5 phases with gates, checkpoints, exit criteria)
   - §6: Concrete regression tests & unit test samples
   - §7: Implementation artifacts (adapters, CMake snippets, inline code examples)
   - §8: Summary & next steps
   
   **Use Case:** Primary reference for all code changes; guards against regressions

### 2. **EXEC_SUMMARY.md** (4.4 KB)
   **Executive Brief — Start Here**
   
   Quick-reference overview:
   - Migration scope table (text, UI, input, build)
   - Phased execution timeline (Phases 0-4, effort estimates)
   - Critical dependency blockers (what can't be removed when)
   - Regression prevention checklist (gates before each phase)
   - Migration data flow diagrams
   - Testing matrix
   - Recommended execution schedule (3-4 weeks)
   
   **Use Case:** Scheduling, approvals, high-level planning

### 3. **rag-core-map.md** (20 KB)
   **Existing Core Architecture (Reference)**
   
   Documentation of current codebase:
   - ECS component inventory
   - System execution order
   - Input flow topology
   - Game state transitions
   
   **Use Case:** Context for understanding current state before migration

---

## 🎯 Key Deliverables

### Migration Mapping (§2, §4)
✅ **Current → LVGL API translation table:**
- Text rendering: `text_draw()` → `lv_label_set_text_fmt()`
- UI rendering: `ui_render_system()` → `lv_scr_load_anim()`
- Input routing: raylib input device → LVGL indev adapter
- Build: SDL2_ttf + raylib → LVGL + generated code

### Phased Checklist (§5)
✅ **5-phase cleanup execution plan:**
- **Phase 0** (2-3 days): LVGL runtime wiring, codegen integration
- **Phase 1** (3-5 days): Input + font loading
- **Phase 2** (10-12 days): Screen-by-screen migration (Title → LevelSelect → Playing → GameOver)
- **Phase 3** (2-3 days): Remove text_renderer, SDL2_ttf cleanup
- **Phase 4** (2-3 days): Validation, cross-platform testing, docs

### Regression Guards (§3, §6)
✅ **Hard dependencies blocking removal:**
- SDL2_ttf: Keep until all 27 `text_draw()` calls → LVGL labels
- `TextContext`: Keep until LVGL font refs cached
- `BeginMode2D/EndMode2D`: Keep until overlays → LVGL objects
- Input dispatcher: **KEEP** (swap source, not logic)

✅ **Checkpoints to verify before advancing:**
- Before Phase 1: codegen compiles, fonts load, LVGL timer runs
- During Phase 2: transitions work, semantic events fire, no leaks
- After Phase 3: zero warnings, full game loop, cross-platform verified

---

## 🔍 How to Use These Artifacts

### For Code Changes (Next Loop)
1. Read **EXEC_SUMMARY.md** (5 min overview)
2. Open **rag-lvgl-map.md** side-by-side with code
3. Follow Phase 0 checklist (§5.0) to gate entry
4. Refer to §2 mapping tables for API equivalents
5. Check §6 regression tests before committing

### For Architecture Review
1. Reference **rag-core-map.md** for current structure
2. Cross-reference with **rag-lvgl-map.md** §2 for changes
3. Use §4 code location mapping to identify impact

### For Testing & Validation
1. Use §6.1 unit test templates
2. Fill out §6.2 visual regression matrix
3. Run §6.3 integration checkpoints after each phase

### For Scheduling
1. Review EXEC_SUMMARY effort estimates
2. Use phase timelines to plan sprint assignments
3. Stage dependencies per phased checklist (§5)

---

## 📊 Current Stack vs. LVGL Stack

### Before (Current)
```
Input → raylib API → InputState ──→ InputEvent ──→ GoEvent/ButtonPressEvent
                     (entt dispatcher, 2-tier)         ↓
                                              Game State + Player Logic

Text → SDL2_ttf Fonts → text_renderer → text_draw() ──→ raylib DrawTextEx()
       (3 sizes)       (text_init)           ↓
                                      ui_render_system (BeginMode2D)

UI → raylib 2D mode → DrawRectangleRec() + ClearBackground()
                      (phase-specific layout)
```

### After (LVGL)
```
Input → LVGL indev device → adapter callback ──→ GoEvent/ButtonPressEvent
        (touch/keyboard)                          ↓
                                          Game State + Player Logic

Text → _lved_project_init() → LVGL font refs ──→ lv_label_create() / set_text_fmt()
       (from resources.zip)    (auto-loaded)      ↓
                                              lv_timer_handler() (auto-render)

UI → LVGL screens → lv_scr_load_anim() ──→ lvgl_load_<phase>_screen()
    (from XML)      (phased navigation)     (automatic render)
```

---

## ✅ Pre-Migration Checklist (Gate for Phase 0)

Before starting any code changes:

- [ ] Review EXEC_SUMMARY.md
- [ ] Confirm rag-lvgl-map.md uploaded to this session state
- [ ] Verify LVGL XML→C codegen pipeline exists: `grep -n "LVGL_CODEGEN" /Users/yashasgujjar/dev/bullethell/CMakeLists.txt`
- [ ] Verify generated files in place: `ls /Users/yashasgujjar/dev/bullethell/build/generated/lvgl/_lved_project.h`
- [ ] Confirm current build passes: `cd /Users/yashasgujjar/dev/bullethell && ./run.sh`
- [ ] Identify team availability for 3-4 week migration

---

## 🚀 Quick Links to Key Sections

| Task | Reference | Section |
|------|-----------|---------|
| Understand current system | rag-core-map.md | §1-3 |
| Map APIs for coding | rag-lvgl-map.md | §2 |
| Plan removal order | rag-lvgl-map.md | §3 |
| Find code locations | rag-lvgl-map.md | §4 |
| Execute Phase X | rag-lvgl-map.md | §5.X |
| Validate changes | rag-lvgl-map.md | §6 |
| CMake setup | rag-lvgl-map.md | §7.3 |
| Effort estimate | EXEC_SUMMARY.md | Migration Scope table |
| High-level plan | EXEC_SUMMARY.md | Phased Execution |

---

## 📝 Notes for Next Loop

- **No production code modified in this task** — all artifacts are reference/planning
- Artifacts stable and ready for Phase 0 execution
- Three phases completed sequentially; gates must be satisfied between phases
- Semantic event queue (GoEvent/ButtonPressEvent) **survives migration** — only input source changes
- Cross-platform testing (Desktop, Web, iOS) required after Phase 3

---

**Artifact Status:** ✅ Complete  
**Ready for:** Phase 0 (Runtime Wiring) in next cleanup loop  
**Questions?** Refer to section numbers above; examples in §7

