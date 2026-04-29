# rguilayout Integration Path

## Overview

This directory contains the integration path for rguilayout-generated UI layouts. The goal is to replace JSON-driven UI with raygui controls and compile-time layout definitions.

## Directory Structure

```
tools/rguilayout/
├── rguilayout.app/         # Vendored rguilayout v4.0 GUI tool
├── templates/              # Custom code generation templates
│   └── embeddable_layout.h # Header-only embeddable template (NO main(), NO RAYGUI_IMPLEMENTATION)
└── README.md

content/ui/screens/
├── title.rgl               # Editable layout sources (.rgl files)
├── tutorial.rgl
├── level_select.rgl
├── gameplay.rgl
├── paused.rgl
├── game_over.rgl
├── song_complete.rgl
└── settings.rgl

app/ui/
├── generated/              # Generated layout code (NOT compiled yet)
│   ├── title_layout.h      # Embeddable generated header (committed)
│   └── ..._layout.h
└── screen_controllers/     # Thin C++ integration glue (compile-safe)
    ├── title_screen_controller.cpp
    └── ..._screen_controller.cpp
```

## Current Status

### ✅ Completed
1. Vendored rguilayout v4.0 tool under `tools/rguilayout/`
2. Created `.rgl` sources for all 8 screens
3. Created custom embeddable template (`tools/rguilayout/templates/embeddable_layout.h`)
4. **Manually** generated embeddable proof artifact: `app/ui/generated/title_layout.h`
5. Runtime integration lives in `app/ui/screen_controllers/`
6. Removed committed standalone exports; only embeddable `*_layout.h` headers remain in source control

### 🚧 Limitations / Blockers
- **rguilayout template substitution is incomplete**: The `--template` flag does not properly substitute all template variables (e.g., `$(GUILAYOUT_INITIALIZATION_C)` is not replaced). This requires **manual** generation of embeddable headers.
- **raygui not in build yet**: `raygui.h` is not included in CMakeLists.txt
- **RAYGUI_IMPLEMENTATION placement**: Need single compilation unit to define `RAYGUI_IMPLEMENTATION`
- **Controller wiring required**: ensure screen controller dispatch in `ui_render_system` stays aligned with generated headers
- **JSON runtime still active**: JSON UI loading still runs; rguilayout path is parallel/inactive

### 🔮 Future Work (deferred to build-integration task)
1. Add `raygui.h` to include path (vendored or vcpkg)
2. Add `#define RAYGUI_IMPLEMENTATION` in a single `.cpp` file
3. Keep `app/ui/screen_controllers/*.cpp` wired in CMakeLists.txt
4. Keep `ui_render_system` dispatch aligned with screen controller render entrypoints
5. Add compile-time flag to switch between JSON UI and rguilayout UI (default OFF)
6. Generate embeddable headers for remaining 7 screens
7. Validate zero-warning build (clang, MSVC, emscripten)

## Generating Embeddable Layouts

### Limitation: Manual Generation Required

Due to rguilayout's incomplete template substitution, embeddable headers must be **manually crafted** from the default standalone output. The process is:

1. Generate standalone code to scratch:
   ```bash
   cd /Users/yashasgujjar/dev/bullethell
   mkdir -p build/rguilayout-scratch
   tools/rguilayout/rguilayout.app/Contents/MacOS/rguilayout \
       --input content/ui/screens/title.rgl \
       --output build/rguilayout-scratch/title_temp.c
   ```

2. Extract initialization and drawing code from the generated `.c` file
3. Wrap into embeddable header using `title_layout.h` as template
4. Place in `app/ui/generated/<screen>_layout.h`

### Embeddable Header Template Structure

See `app/ui/generated/title_layout.h` for reference. Key requirements:

- **Header-only**: `#ifdef TITLE_LAYOUT_IMPLEMENTATION` guard
- **No main()**: Remove `int main()` and event loop
- **No RAYGUI_IMPLEMENTATION**: Caller defines it elsewhere
- **State struct**: Expose anchors and button/control state
- **Init function**: Returns initialized state struct
- **Render function**: Takes `state*`, draws controls, updates button presses
- **C++/C compatible**: `extern "C"` guards

### Automated Generation (future)

If rguilayout's template system is fixed or we create a post-processor:

```bash
# Future desired command (NOT working yet)
tools/rguilayout/rguilayout.app/Contents/MacOS/rguilayout \
    --input content/ui/screens/title.rgl \
    --output app/ui/generated/title_layout.h \
    --template tools/rguilayout/templates/embeddable_layout.h
```

## Screen Controller Contract

Screen controllers live in `app/ui/screen_controllers/` and should:
- ✅ Include generated layout headers from `app/ui/generated/*_layout.h`
- ✅ Call generated Init/Render functions
- ✅ Translate button presses to game actions/events
- ✅ Read game state for dynamic text/values
- ❌ **Do NOT** copy layout rectangles into ECS components
- ❌ **Do NOT** create layout cache structs
- ❌ **Do NOT** spawn widget entities

## CMake Integration (future)

When wiring into the build:

```cmake
# 1. Add raygui include path
target_include_directories(shapeshifter PRIVATE ${RAYGUI_INCLUDE_DIR})

# 2. Add generated layout headers (header-only, no .cpp)
# Already included via adapters, no explicit target needed

# 3. Add screen controllers to build
target_sources(shapeshifter PRIVATE
    app/ui/screen_controllers/title_screen_controller.cpp
    # ... other screen controllers as they land
)

# 4. Define RAYGUI_IMPLEMENTATION in ONE compilation unit
# Option A: ui_render_system.cpp (if it becomes C-only raygui host)
# Option B: new app/ui/raygui_impl.c file
```

## Safety Rules

1. **Never include standalone generated files** (they have `main()` and `RAYGUI_IMPLEMENTATION`)
2. **Never commit standalone outputs**; if needed for extraction, generate only under `build/rguilayout-scratch/`
3. **Only compile embeddable headers** via adapters
4. **Define RAYGUI_IMPLEMENTATION once** in the entire binary
5. **Keep JSON UI active** until rguilayout path is validated (no deletion yet)

## Validation Checklist (future PR)

- [ ] `./run.sh` builds zero-warning (clang)
- [ ] `./run.sh test` passes all tests
- [ ] WASM build compiles (emscripten)
- [ ] Title screen renders with rguilayout when feature flag enabled
- [ ] Button clicks dispatch correct game actions
- [ ] No layout data in ECS components
- [ ] No `HudLayout`/`LevelSelectLayout`/`OverlayLayout` structs remain
- [ ] JSON UI path still works when feature flag disabled

## References

- Design spec: `design-docs/raygui-rguilayout-ui-spec.md`
- rguilayout docs: `tools/rguilayout/USAGE.md`
- Game flow: `design-docs/game-flow.md`
