# rguilayout Portable C Integration Spec

## Status

Active — governs the integration boundary between rguilayout-authored `.rgl` layout sources, the project's codegen script, and the entity-driven UI runtime in `app/systems/`.

## Overview

rguilayout is a visual editor that produces `.rgl` project files describing UI controls (rectangles, labels, buttons, dummy frames) with anchors and styles. **This project does not consume `rguilayout`'s native code-export path.** Instead, `tools/rguilayout/codegen.py` parses the committed `.rgl` files directly and emits per-screen entity-spawner C++ sources under `app/systems/generated/`. Each control row in the `.rgl` becomes one ECS entity carrying atomic components plus per-screen and per-kind existential tags. The runtime UI is then a uniform pass over those entities — there is no screen-by-screen render dispatch.

**Key principle:** The `.rgl` file is the source of truth for UI layout. Codegen output is regenerated deterministically at CMake configure time; hand-written code never creates UI entities outside the spawner contract.

## Goal: Codegen-Emitted Entity Spawners as the Integration Boundary

The codegen output — per-screen `spawn_<screen>_screen(reg)` / `despawn_<screen>_screen(reg)` functions emitted to `app/systems/generated/<screen>_screen.cpp` — is the single contract between:

- **Upstream (authoring + codegen):** rguilayout `.rgl` files in `content/ui/screens/` and `tools/rguilayout/codegen.py`.
- **Downstream (runtime):** `app/systems/screen_lifecycle_system.cpp` (calls the spawners on phase tag changes) and `app/systems/ui_render_system.cpp` / `app/systems/ui_update_system.cpp` (consume the resulting entities).

```text
[rguilayout authoring app]
         |
         | author in GUI, save .rgl
         v
[content/ui/screens/*.rgl] (editable source — committed)
         |
         | tools/rguilayout/codegen.py runs at CMake configure time
         v
[app/systems/generated/<screen>_screen.cpp + screen_spawners.h] (committed codegen output)
         |
         | linked into shapeshifter_lib
         v
[app/systems/screen_lifecycle_system.cpp] (calls spawn_<screen>_screen on
                                           matching GamePhase*Tag activation)
         |
         | UI entities emplaced into the registry
         v
[UiLabelTag / UiButtonTag / UiDummyRecTag / LaneButtonTag / EnergyBarTag
 entities, each carrying UiPosition / UiBounds (+ UiLabel / OnPress / kind
 overrides) plus a per-screen tag]
         |
         | per-frame
         v
[app/systems/ui_render_system.cpp]  single pass over UI entities
[app/systems/ui_update_system.cpp]  hit-test UiButtonTag → invoke OnPress action
```

**Contract:** The spawner sources are **final** codegen output. They are not hand-edited, not post-processed, and are regenerated whenever the matching `.rgl` changes. CMake's `add_custom_command(OUTPUT …)` for codegen declares a `DEPENDS` set covering every `.rgl` and the script itself, so a `cmake --build` is sufficient to keep them in sync.

## Source-of-Truth Flow

### 1. Author in rguilayout (Offline)

- Open the vendored authoring app under `tools/rguilayout/rguilayout.app/` (or any installed `rguilayout` build).
- Load or create `content/ui/screens/<screen>.rgl`.
- Arrange raygui controls, anchors, and geometry.
- Save the `.rgl` and commit it to the repo.
- **Do not use rguilayout's built-in `--template` / code-export feature.** Codegen handles the C++ emission. The native rguilayout exporter is bypassed entirely (see "Why not rguilayout's native code export?" below).

### 2. Codegen-Emitted Entity Spawner

`tools/rguilayout/codegen.py` parses each `.rgl` and writes `app/systems/generated/<screen>_screen.cpp`:

- One self-contained C++ source per screen. No `.h` per screen — the entire surface is the two function declarations in `screen_spawners.h`.
- Includes only `components/actions.h`, `components/ui.h`, `tags/tags.h`, and `<entt/entt.hpp>` (and `<string>` where labels are present).
- `spawn_<screen>_screen(reg)` emplaces one entity per control row, attaching:
  - The per-screen tag (e.g. `TitleScreenTag`, `PausedScreenTag`) — the membership marker used by despawn.
  - A per-kind tag (`UiLabelTag` for text, `UiButtonTag` for clickable buttons, `UiDummyRecTag` for non-interactive frames; gameplay-specific kinds such as `LaneButtonTag` / `EnergyBarTag` for HUD).
  - Atomic `UiPosition`, `UiBounds`, and (for labels and dynamic-text slots) `UiLabel`.
  - For buttons, `OnPress { action_fn }` where `action_fn` is one of the small set of `void (entt::registry&, entt::entity)` handlers defined in `ui_update_system.cpp` or peer systems. The mapping `.rgl` button name → action function is enforced by the `ActionId` enum (`app/components/actions.h`); codegen verifies every button name appears in the enum and exits non-zero if not.
- `despawn_<screen>_screen(reg)` issues a single `reg.view<<ScreenTag>>()` walk and destroys every matched entity. Membership in the per-screen tag is the entire signal — there is no manual entity list.

Output is deterministic. Running codegen on identical inputs produces byte-identical output. CMake re-runs codegen only when an input changes (`.rgl`, the codegen script, or `actions.h`).

### 3. Runtime Consumers

The codegen output is consumed by three systems:

- `app/systems/screen_lifecycle_system.cpp` — owns a table mapping each `GamePhase*Tag` to its `(spawn, despawn)` pair (plus existence probes for the matching screen tag). Once per frame, the system walks the table and converges the entity set to match the active phase tag. The table is the only place that names individual screens; there is no `switch (phase)` anywhere in the UI pipeline.
- `app/systems/ui_render_system.cpp` — single render pass over `UiLabelTag` / `UiButtonTag` / `UiDummyRecTag` and the gameplay-HUD-specific kind tags. Each entity is drawn using its atomic `UiPosition` / `UiBounds` plus the per-kind draw callback selected by tag.
- `app/systems/ui_update_system.cpp` — touch / mouse hit testing against `UiButtonTag` (and `LaneButtonTag` for shape buttons). On a hit, the entity's `OnPress.action` is invoked and emits the semantic event the rest of the game consumes (e.g. `ShapePressCircleEvent`, `MenuConfirmEvent`, request `NextPhasePausedTag`).

Per-screen bind systems (`gameplay_hud_bind_system`, `game_over_scoreboard_bind_system`, etc.) read game state and **mutate** atomic component data on spawner-emitted entities — e.g. write the current score string into a label's `UiLabel.text`, or set a per-button colour override. Bind systems never create or destroy UI entities.

### 4. RAYGUI_IMPLEMENTATION Ownership

raygui is a header-only single-file library that compiles its implementation only when `#define RAYGUI_IMPLEMENTATION` is set in exactly one translation unit. That TU is `app/util/raygui_impl.cpp`. CMake attaches the macro via `COMPILE_DEFINITIONS RAYGUI_IMPLEMENTATION` on that source and excludes it from unity batching:

```cmake
set_source_files_properties(
    app/util/raygui_impl.cpp
    PROPERTIES
        COMPILE_DEFINITIONS RAYGUI_IMPLEMENTATION
        SKIP_UNITY_BUILD_INCLUSION TRUE
)
```

Every other UI consumer TU (`ui_render_system.cpp`, `ui_update_system.cpp`, `screen_lifecycle_system.cpp`, each codegen-emitted spawner, and each bind system) pulls in raygui headers normally without defining the implementation macro.

## Why Not rguilayout's Native Code Export?

rguilayout's built-in `--template` flag generates standalone C/C++ wrappers (one `LayoutState` struct + `Init` / `Render` functions per screen) intended for short-lived prototypes. Adopting that output would re-introduce a screen-by-screen render dispatch (`switch (phase)` calling `RenderTitle` / `RenderPause` / …), which conflicts with Fabian existential processing (Principle 1 in `.squad/decisions.md`). The project's codegen path produces flat ECS entity emplacements instead, which the registry's view queries dispatch on without any central switch.

Concrete reasons the project owns the codegen rather than relying on rguilayout's exporter:

- **Existential dispatch:** Per-kind tags (`UiLabelTag` / `UiButtonTag` / …) replace the per-screen function-pointer table the native exporter would generate.
- **Action enum integrity:** Codegen cross-checks every button name in every `.rgl` against `ActionId` in `app/components/actions.h`. A button whose name is not in the enum aborts the build with a clear diagnostic; this is impossible to enforce from the native exporter side.
- **Single-pass rendering:** All UI entities go through one render pass. The native exporter would produce per-screen rendering code that must coexist with the gameplay-HUD custom controls (energy bar, shape buttons), creating two parallel rendering paths.
- **Deterministic, reviewable output:** The committed codegen output is plain C++ that a reviewer can diff. The native exporter's template substitution is harder to audit and depends on rguilayout's release cadence.
- **Zero runtime dependency on rguilayout:** Nothing in the runtime imports a rguilayout header. The authoring app stays a developer-only tool; CI never installs or runs it.

## API Shape: Per-Screen Spawner Pair

Every codegen-emitted source follows the same shape:

```cpp
// app/systems/generated/<screen>_screen.cpp  (auto-generated; do not edit)
#include "components/actions.h"
#include "components/ui.h"
#include "tags/tags.h"
#include <entt/entt.hpp>

void spawn_<screen>_screen(entt::registry& reg) {
    {
        auto e = reg.create();
        reg.emplace<<ScreenTag>>(e);
        reg.emplace<UiLabelTag>(e);                    // (or UiButtonTag / UiDummyRecTag)
        reg.emplace<UiPosition>(e, x, y);
        reg.emplace<UiBounds>(e, w, h);
        reg.emplace<UiLabel>(e, "Static text");        // dynamic text uses an empty UiLabel
                                                       // mutated by a bind system
        // For buttons:
        // reg.emplace<OnPress>(e, &action_fn);
    }
    // … one block per control row in the .rgl …
}

void despawn_<screen>_screen(entt::registry& reg) {
    auto view = reg.view<<ScreenTag>>();
    reg.destroy(view.begin(), view.end());
}
```

Forward declarations for every pair live in `app/systems/generated/screen_spawners.h`, which is the single header `screen_lifecycle_system.cpp` includes.

**No `LayoutState` struct, no `Init` / `Render` functions, no `GuiLayout_*` symbols** — the project does not consume rguilayout's native template output, so those names do not appear anywhere in `app/`.

## Event-Consumer Design: UI Effects Outside Layout

Dynamic visual effects (screen shake, flashes, colour tints) are **render-time state**, not layout geometry. They live in dedicated bind systems and per-entity override components, applied around the standard render pass:

```cpp
// Approach-ring proximity feedback: per-LaneButtonTag entity component
struct ApproachRing {
    float ratio;          // 0..1 — written each frame by approach_ring_envelope_system
    Color color;          // tier colour (perfect / great / ok / miss)
    bool  active;
};

// Per-frame in ui_render_system:
// for each LaneButtonTag entity, draw the button using its UiPosition/UiBounds,
// then if ApproachRing.active, draw the shrinking ring overlay.
```

Layout geometry is **static** between `.rgl` edits; effects are **applied around the render pass** by mutating atomic components or drawing overlays. This keeps codegen output focused and avoids coupling layout data to game effects.

## File Layout and Naming

```text
tools/rguilayout/
  rguilayout.app/             # Vendored rguilayout authoring GUI (developer-only).
  codegen.py                  # .rgl → entity-spawner C++ emitter.
  INTEGRATION.md              # Tool-side integration notes.
  USAGE.md                    # Authoring workflow.

content/ui/screens/
  title.rgl                   # Editable layout source (rguilayout project file).
  tutorial.rgl
  level_select.rgl
  gameplay.rgl
  paused.rgl
  game_over.rgl
  song_complete.rgl
  settings.rgl

app/systems/
  generated/
    title_screen.cpp          # Codegen output: spawn_/despawn_ pair.
    tutorial_screen.cpp
    level_select_screen.cpp
    gameplay_screen.cpp
    paused_screen.cpp
    game_over_screen.cpp
    song_complete_screen.cpp
    settings_screen.cpp
    screen_spawners.h         # Forward declarations for every pair.

app/util/
  raygui_impl.cpp             # Single RAYGUI_IMPLEMENTATION TU; excluded from unity.
  tutorial_dodge_hint.h       # Hand-written tutorial helper (runtime touch/keyboard hint copy).
```

**Naming conventions:**

- `.rgl` files: `<screen>.rgl`, lowercase with underscores.
- Codegen output: `app/systems/generated/<screen>_screen.cpp` and `<screen>_screen.cpp` exposes `spawn_<screen>_screen` / `despawn_<screen>_screen`.
- Per-screen tags: `<Screen>ScreenTag` in `app/tags/tags.h` (CamelCase derived from the file name).

**Current pattern:** Every migrated screen is one `.rgl` + one codegen-emitted `.cpp` + one row in the `screen_lifecycle_system.cpp` lifecycle table + one entry in `app/tags/tags.h`. Adding a new screen requires touching exactly those four sites; no boilerplate render-dispatch code is needed.

## Strict Non-Goals

These are explicitly out of scope for the codegen integration:

1. **Hand-written widget mirrors:** Do NOT create `UIWidgetTag` / `UIRectangle` / `UIButtonState` entities to mirror codegen output. The codegen output IS the entity set.
2. **Layout-data copying:** Do NOT read codegen-emitted rectangles and store them in `reg.ctx()` layout caches or hand-written structs. If a system needs a rectangle, query the spawner-emitted entity directly.
3. **Hand-editing codegen output:** `app/systems/generated/*_screen.cpp` and `screen_spawners.h` are final. Any layout change is made in the `.rgl` and codegen is regenerated.
4. **Adopting rguilayout's native exporter:** Do not add a CMake target for the native `--template` flag. The project's codegen path is the only emitter.
5. **Per-screen render dispatch:** Do not add `switch (phase)` / per-screen `render_*` functions in `ui_render_system.cpp`. The renderer queries by per-kind tag, not by screen.
6. **No committed standalone scratch files:** Any temporary standalone rguilayout exports must live in `build/rguilayout-scratch/` (or an ignored scratch path) and must not be committed.

## Migration History: Native Exporter → Codegen + Entity Spawners

This integration replaced two earlier UI shapes in sequence:

1. **Adapter-pattern files** (`app/ui/adapters/<screen>_adapter.cpp`) — removed before any official release of this layer.
2. **Per-screen render-callback files** (one C++ TU per screen, deleted in #1308 once #1287 entity-driven UI shipped end-to-end). Staging shape used while the rguilayout-native-exporter approach was prototyped. Today there is no per-screen render-callback layer.

The current pattern (codegen + entity spawner + uniform render pass) is the only shape used; the historical `app/ui/` folder no longer exists (relocated to `app/systems/generated/` in #1325 per the Section-7 folder allowlist).

### Incorrect Pattern (Avoid)

If you encounter code that rebuilds layout state or copies codegen-emitted rectangles into hand-written structs, this is incorrect and must be fixed:

```cpp
// ❌ WRONG: Copying spawner-emitted rectangles into a cache
TitleLayout cached_layout = rebuild_layout_from_constants(reg);

// ❌ WRONG: Storing layout in ECS context
reg.ctx().emplace<TitleLayoutCache>(cached_layout);
```

### Correct Pattern

Bind systems mutate atomic components on the spawner-emitted entities:

```cpp
// ✅ CORRECT: gameplay_hud_bind_system writes the current score into the
// score-label entity's UiLabel.text every frame. Binding is by position
// match (the codegen bakes the .rgl slot's exact x/y into the spawner, so
// the bind system finds its target by querying labels at that position
// inside the gameplay screen view).
void gameplay_hud_bind_system(entt::registry& reg) {
    const auto& score = reg.ctx().get<ScoreState>();
    for (auto [e, label, pos] :
            reg.view<UiLabel, UiPosition, GameplayHudTag>().each()) {
        if (pos.x == kScoreSlotX && pos.y == kScoreSlotY) {
            ui_label_set(label, format_score(score.value));
        }
    }
}

// ✅ CORRECT: An OnPress action emits the semantic event and (optionally)
// requests a phase transition via the same tag mechanism used elsewhere.
void pause_button_action(entt::registry& reg, entt::entity /*entity*/) {
    request_phase_transition<NextPhasePausedTag>(reg);
}
```

## Build and Test Notes

### CMake Integration

The build wires the pipeline as follows:

```cmake
# 1. Discover .rgl sources.
file(GLOB RGL_SOURCES CONFIGURE_DEPENDS content/ui/screens/*.rgl)

# 2. Codegen: .rgl → app/systems/generated/<screen>_screen.cpp + screen_spawners.h.
add_custom_command(
    OUTPUT  ${RGUILAYOUT_GENERATED_SOURCES}
    COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/rguilayout/codegen.py
            --output-dir   ${CMAKE_SOURCE_DIR}/app/systems/generated
            --actions-header ${CMAKE_SOURCE_DIR}/app/components/actions.h
            ${RGL_SOURCES}
    DEPENDS ${RGL_SOURCES} ${CMAKE_SOURCE_DIR}/tools/rguilayout/codegen.py
)

# 3. raygui implementation owner — single TU, excluded from unity.
set_source_files_properties(
    app/util/raygui_impl.cpp
    PROPERTIES
        COMPILE_DEFINITIONS RAYGUI_IMPLEMENTATION
        SKIP_UNITY_BUILD_INCLUSION TRUE
)

# 4. Link codegen output + raygui impl into shapeshifter_lib.
add_library(shapeshifter_lib STATIC
    ${SYSTEM_SOURCES} ${UTIL_SOURCES} ${ENTITY_SOURCES}
    ${RGUILAYOUT_GENERATED_SOURCES}
)
```

### Compiler Warnings

- Codegen output and raygui implementation must compile **zero warnings** with `-Wall -Wextra -Werror` (Clang on Linux/macOS/Windows, Emscripten).
- If a generated source produces warnings, fix the codegen template — never hand-edit the output.

### Testing

- **Unit tests** for `ui_update_system` action handlers: each `OnPress`-bound function is exercised directly by emplacing the matching `OnPress` and entity, then asserting the semantic event / phase transition fires.
- **Lifecycle tests** for `screen_lifecycle_system`: assert that toggling `GamePhase*Tag` ctx mirrors causes the corresponding `<Screen>ScreenTag` entity view to populate / clear.
- **Codegen verification** in `tools/test_rguilayout_codegen.py`: parses sample `.rgl` files and asserts the output matches a golden tree, plus rejects `.rgl` files whose button names are not in `ActionId`.

## Validation Checklist

- [ ] All screen `.rgl` files are authored and committed in `content/ui/screens/`.
- [ ] All `<screen>_screen.cpp` files are committed in `app/systems/generated/` and were produced by running codegen on the corresponding `.rgl`.
- [ ] No codegen output is hand-edited; any layout change regenerates via `cmake --build`.
- [ ] Every button name in every `.rgl` appears in `app/components/actions.h`'s `ActionId` enum.
- [ ] `screen_lifecycle_system.cpp`'s lifecycle table has one row per migrated screen.
- [ ] `CMakeLists.txt` defines `RAYGUI_IMPLEMENTATION` on exactly one source (`app/util/raygui_impl.cpp`) and excludes it from unity.
- [ ] No system under `app/` creates UI entities outside the codegen-emitted spawners.
- [ ] Build is warning-free (native and WASM).
- [x] Legacy adapter / per-screen-render-callback folders have been deleted (#1308); the entity-driven UI is the only shape.

## Open Decisions

1. **`tutorial_dodge_hint.h`:** Resolved — moved to `app/util/tutorial_dodge_hint.h` (#1317 sub-PR 2). The helper is header-only `inline` and consumed by `app/systems/tutorial_dodge_hint_bind_system.cpp` plus two test TUs; `app/util/` is the right home per the Section-7 folder allowlist.
2. **Dynamic-content slots:** For per-frame text updates (score, selected song), bind systems mutate `UiLabel.text` on spawner-emitted entities. The convention works; whether to extend it to per-frame `UiBounds` mutation (animations) is open.
3. **Platform guards:** Currently a single shared spawner emits every entity; platform-only entities (e.g. web-only exit button) are gated by visibility-bind systems that toggle render or hit-test inclusion based on the platform. Confirming this scales to more platform splits is a future task.
4. **Preloading:** Spawner sources are header-light and parse-light; spawn cost is negligible compared to the rest of the per-phase setup. Pre-allocation has not been needed.

## Summary

**Codegen-emitted entity spawners are the primary integration contract.** rguilayout produces editable `.rgl` files; `tools/rguilayout/codegen.py` parses them and emits per-screen spawner functions that emplace UI entities into the registry. `screen_lifecycle_system` swaps entity sets when the active `GamePhase*Tag` mirror changes, and `ui_render_system` / `ui_update_system` walk the entities once per frame. No per-screen render-dispatch code, no `LayoutState` structs, no hand-written widget mirrors. The codegen output is the single source of UI entity creation under `app/`.
