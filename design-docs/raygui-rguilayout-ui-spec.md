# raygui + rguilayout UI Migration Spec

## Status

Implemented. All migration phases described below have shipped; this spec now documents the as-built rguilayout / raygui screen-controller architecture.

This spec replaced the legacy JSON-driven UI layout path with raygui controls and rguilayout-authored/generated layout files. The current implementation lives under `app/ui/generated/`, `app/ui/screen_controllers/`, and `app/systems/ui_render_system.cpp` (raygui screen-controller dispatch + render-pass glue).

Current runtime note: proximity rings are active HUD timing feedback in
`gameplay_hud_screen_controller.cpp` and are specified by
`design-docs/rhythm-spec.md`. The migration preserved this behavior; the
historical `approach_ring` JSON layout fields (which lived in the now-removed
`content/ui/screens/gameplay.json`) were intentionally not ported and are not
the active C++ proximity-ring behavior.

Current directive: the rguilayout application is vendored under `tools/rguilayout/`; `.rgl` files have replaced the legacy `content/ui` JSON layout sources; exported `.h` files live under `app/ui/generated/` (the export pattern in use today is header-only — no `.c` exports are committed); the headers are compiled in via the screen-controller `.cpp` files under `app/ui/screen_controllers/`.

The Excalidraw tool currently returns an empty scene, so Excalidraw is not a usable source of truth for this spec. If Excalidraw is restored later, it should be used as visual reference only; rguilayout `.rgl` files remain the authoring source for compiled UI layout.

## Goals

1. Replace hand-authored runtime JSON UI layout files with rguilayout `.rgl` authoring files and generated header exports compiled in via screen controllers.
2. Use raygui for menu, overlay, settings, pause, and HUD UI controls where appropriate.
3. Keep layout geometry contained in rguilayout exported files. Do not mirror exported rectangles, anchors, widget IDs, or hit targets into ECS components, ECS entities, or `reg.ctx()` layout caches.
4. Preserve existing `GamePhase`-driven screen-controller dispatch and dispatcher-driven game commands.
5. Keep UI geometry contained in generated rguilayout exports. The legacy duplicators (`ui_layout_cache.h`, `ui_button_spawner.h`, and the `UIElementTag` / `UIText` / `UIButton` / `UIShape` widget components) have already been removed; do not re-introduce equivalent project-owned layout structures.

## Non-goals

- Do not change gameplay scoring, energy, input semantics, level selection rules, or song result computation.
- Do not re-introduce the stale `approach_ring` layout data that lived in the removed `content/ui/screens/gameplay.json`. Preserve the active C++ proximity-ring HUD behavior unless a separate gameplay/design decision removes it.
- Do not create custom ECS layout components/entities to represent rguilayout data.
- Do not require rguilayout to be installed in CI or at runtime (CI does not run the authoring tool).

## Core decision: exported files own layout data

The rguilayout export is the runtime boundary for UI layout data. Generated headers are committed under `app/ui/generated/` and consumed by screen controllers under `app/ui/screen_controllers/`.

The rguilayout tool itself lives in:

```text
tools/rguilayout/
```

For each migrated screen, commit:

| File | Role | Runtime behavior |
|---|---|---|
| `content/ui/screens/<screen>.rgl` | Editable rguilayout source replacing `content/ui/screens/<screen>.json` | Not compiled |
| `app/ui/generated/<screen>_layout.h` (e.g. `title_layout.h`) | Generated C/C++ header with `GuiLayout_*` symbols, anchors, and a `<Screen>LayoutState` struct | Header-only; included by the matching screen controller `.cpp` and compiled in transitively |

The current export pattern is header-only — there are no committed generated `.c` files. The screen controller TU that includes the layout header acts as the implementation owner.

Recommendation: keep screen layouts under `content/ui/screens/*.rgl` because the existing split already stores screen JSON in `content/ui/screens/` and keeps `content/ui/routes.json` at the root. Use root-level `content/ui/*.rgl` only if a future non-screen/global layout source is introduced. Route data is not layout geometry; do not replace `routes.json` with an `.rgl` unless routing is moved to another non-layout source.

The generated headers contain the runtime layout geometry and generated `GuiLayout_*`-style APIs. Hand-written code may call generated APIs and pass dynamic state/text into them, but it must not copy generated rectangles into project-owned structs or ECS storage.

Disallowed:

- `RaguiAnchors<ScreenTag>` or similar ECS/ctx anchor structs.
- Rebuilding `HudLayout`, `LevelSelectLayout`, or `OverlayLayout` from rguilayout constants.
- `UIElementTag`-style entities for generated widgets.
- Legacy UI hit-test entities whose only purpose is raygui widget hit testing.
- Hand-written normalized-coordinate tables that duplicate `.rgl` data.

Allowed:

- Existing game/menu state in ECS or ctx: active phase/screen, score, high score, energy, selected song, selected difficulty, settings values, dispatcher/services.
- Thin non-ECS screen controllers that include generated headers, call generated layout APIs, bind runtime text/state, apply platform guards, and translate raygui return values into existing events.
- Custom raygui-style controls implemented as drawing/input functions, provided their placement comes from generated rguilayout exports and not duplicated ECS layout state.

## Runtime architecture

```text
tools/rguilayout/
        |
        | author/edit/export on developer machine
        v
content/ui/screens/<screen>.rgl
        |
        | export generated code
        v
app/ui/generated/<screen>_layout.h
        |
        | included by the matching screen controller TU
        v
app/ui/screen_controllers/<screen>_screen_controller.cpp
        |
        | called by UI render path
        v
ui_render_system()
        |
        | raygui return values become existing ButtonPressEvent / navigation actions
        v
existing dispatcher + game state systems
```

Screen controllers and `ui_render_system` are responsible for screen dispatch and widget interaction. They do not load JSON, spawn widget entities, or populate layout caches for rguilayout screens.

`ui_render_system` switches on the active screen and calls the relevant screen controller. The controller calls generated layout functions directly. If the generated header exposes named controls, slots, or rectangles, the controller may use those generated symbols directly; the key rule is that the values stay in generated files and are not persisted into ECS or copied into hand-written layout tables.

## Screen migration scope

| Screen | Migration target | Notes |
|---|---|---|
| Title | Generated layout + raygui screen controller | Preserve start/confirm behavior. Exit stays platform-gated off web. |
| Tutorial | Generated layout + screen controller | Runtime demo shapes may be drawn by custom functions into generated slots. |
| Level Select | Generated layout + screen controller | Song list may remain data-driven, but card placement must come from generated layout or generated list slots, not `LevelSelectLayout`. |
| Gameplay HUD | Generated layout + screen controller / custom controls | HUD is in scope. Use generated placement for score, high score, pause, energy, lane divider, and shape buttons. |
| Paused | Generated layout + screen controller | Preserve dim overlay behavior; overlay geometry/color must not come from JSON caches. |
| Game Over | Generated layout + screen controller | Dynamic score/high score/death reason is bound at render time. |
| Song Complete | Generated layout + screen controller | Dynamic score/high score/stat table is bound at render time. |
| Settings | Generated layout + screen controller | Wire only when `GamePhase::Settings` / route support exists; runtime toggle text is state-derived. |

## HUD treatment

The HUD is part of this refactor. Stale JSON `approach_ring` layout fields are
out of scope, but the active proximity-ring timing cue is in scope for behavior
parity and should remain driven by gameplay state.

HUD controls should be implemented as raygui or raygui-style immediate-mode functions, with layout placement supplied by generated rguilayout exports:

| HUD element | Implementation |
|---|---|
| Score / high score | `GuiLabel` or existing text draw called from the HUD screen controller using generated slots. |
| Pause button | Stock `GuiButton` is acceptable. |
| Energy bar | `GuiProgressBar` or project `GuiEnergyBar(...)` custom control. LOW blink/text/border behavior stays dynamic, but bounds come from generated layout. |
| Shape buttons | Project `GuiShapeButton(...)` custom controls are acceptable for circular hit testing, colorblind glyphs, and shape-specific drawing. Placement comes from generated layout. |
| Lane divider | Custom draw using generated layout position. |
| Proximity rings | Preserve active timing-cue behavior; do not port stale JSON `approach_ring` layout data. |

Custom controls are not ECS entities. They are immediate-mode UI functions called during rendering. They may read game state and return clicks/commands, but they do not own persistent layout data.

## Screen controller boundary

Screen controllers live under:

```text
app/ui/screen_controllers/
  title_screen_controller.cpp/.h
  tutorial_screen_controller.cpp/.h
  level_select_screen_controller.cpp/.h
  gameplay_hud_screen_controller.cpp/.h
  paused_screen_controller.cpp/.h
  game_over_screen_controller.cpp/.h
  song_complete_screen_controller.cpp/.h
  settings_screen_controller.cpp/.h
```

A shared `app/ui/screen_controllers/screen_controller_base.h` provides the common dispatch surface used by `ui_render_system`. One screen controller TU also acts as the `RAYGUI_IMPLEMENTATION` owner (currently `title_screen_controller.cpp`).

Screen controllers may contain:

- `extern "C"` / C++ include boundaries for generated headers.
- Calls to generated `GuiLayout_*` functions or generated control symbols.
- Runtime text binding via existing state/resolvers.
- Platform guards such as web-only/desktop-only visibility.
- Viewport transform setup/reset if generated coordinates need virtual-screen mapping.
- Dispatcher/event glue translating UI clicks into existing commands.
- Short-lived stack-local state required by raygui/generated APIs.

Screen controllers must not contain:

- Copied `Rectangle` constants from generated files.
- Normalized coordinate constants duplicated from `.rgl`.
- Rebuilt `HudLayout`, `LevelSelectLayout`, `OverlayLayout`, or equivalent layout structs.
- `reg.ctx()` layout caches.
- ECS components/entities for widgets or hit targets.
- Persistent widget state that is not actual menu/game state.

## Dynamic text and state binding

Dynamic UI values stay runtime-driven:

- Score / high score.
- Energy value and LOW cue.
- Current shape and selected shape.
- Selected song and difficulty.
- Game-over reason.
- Song-complete timing/stat table.
- Settings toggle labels and audio offset.

The screen controller reads existing game state/resolvers and passes current values into generated layout calls or uses generated slots to draw dynamic controls. This preserves dynamic behavior without copying layout data into ECS.

If rguilayout's generated API cannot directly accept a dynamic string for a control, prefer one of these approaches:

1. Author the control as a named empty/placeholder slot in `.rgl`, expose it in generated output, and draw the dynamic raygui/custom control from the screen controller using that generated slot.
2. Adjust the `.rgl`/export pattern so the generated file exposes a generated hook/state struct for dynamic labels.

Do not solve this by creating project-owned layout constants or ECS layout mirrors.

## Input and event flow

raygui controls perform immediate-mode hit testing. The screen controller converts successful clicks/activations into the same semantic events the game already understands.

Examples:

- Title start -> existing confirm/start event path.
- Pause resume/menu -> existing pause/menu actions.
- Level select difficulty/start -> existing level-selection state/actions.
- HUD pause -> existing pause action.
- Shape button click/touch -> existing player shape-change path.

For migrated raygui controls, do not spawn parallel ECS hit-test entities. Screen controllers emit semantic UI/gameplay actions directly as they take ownership of those controls.

## Build integration

Generated headers are wired into the build as part of `shapeshifter_lib`:

- `tools/rguilayout/` is the vendored authoring/tooling location and is not built or run by CMake/CI.
- `.rgl` layout sources live in `content/ui/screens/` unless a future global/root layout source is justified.
- Generated `.h` exports live under `app/ui/generated/` and are picked up via `target_include_directories(... app/ui)`; consumers `#include "../generated/<screen>_layout.h"`.
- Screen controller `.cpp` files under `app/ui/screen_controllers/` are globbed into `shapeshifter_lib` (`UI_SCREEN_CONTROLLER_SOURCES`) and consume the generated headers.
- One screen controller TU owns `RAYGUI_IMPLEMENTATION` and is excluded from unity batching to keep the raygui implementation isolated; today this is `app/ui/screen_controllers/title_screen_controller.cpp`.

The native build (`shapeshifter`, `shapeshifter_tests`) and WASM build compile the generated headers transitively through the screen controllers and stay warning-clean under `-Wall -Wextra -Werror` / `/W4 /WX`.

## CI and cache

- CI does not run the rguilayout authoring application.
- Native CI and WASM CI compile screen controllers (and therefore the generated headers they include) as part of `shapeshifter_lib`.
- CMake cache keys cover the screen controller globs and the generated headers via the standard `CONFIGURE_DEPENDS` glob; no extra cache key changes are required when a new generated header is added alongside a new controller.
- No WASM preload changes are needed unless a later design introduces external UI assets. This spec does not require an external `.rgs` style file.

## Migration phases

### Phase 1: Authoring source layout — done

1. Use the vendored rguilayout application under `tools/rguilayout/`.
2. Store screen `.rgl` files as `content/ui/screens/<screen>.rgl`, replacing the matching `content/ui/screens/<screen>.json` layout source as each screen migrates.
3. Keep `content/ui/routes.json` until routing is moved to code or another non-layout source.
4. Export generated headers to `app/ui/generated/<screen>_layout.h`.
5. Wire generated headers and screen controllers into `shapeshifter_lib`.

### Phase 2: First screen authoring proof — done

1. Authored and exported the Title screen: `content/ui/screens/title.rgl` + `app/ui/generated/title_layout.h`.
2. Tap/Enter start behavior and web exit-button gating are preserved by `app/ui/screen_controllers/title_screen_controller.cpp`.
3. Generated header and controller compile into `shapeshifter_lib`.

### Phase 3: Build integration — done

raygui/generated-code build support, screen controller compilation, native CI coverage, and WASM CI coverage are in place. `RAYGUI_IMPLEMENTATION` ownership lives on `title_screen_controller.cpp` and that TU is excluded from unity batching.

### Phase 4: Menus and overlays — done

Tutorial, Paused, Game Over, Song Complete, Level Select, and Settings each have a generated header under `app/ui/generated/` and a matching screen controller. Their JSON/layout-cache/widget-entity dependencies have been removed.

### Phase 5: Gameplay HUD — done

HUD placement is driven by `app/ui/generated/gameplay_hud_layout.h` and `app/ui/screen_controllers/gameplay_hud_screen_controller.cpp`. Shape buttons and the energy bar are implemented as raygui-style custom immediate-mode controls. Stale approach-ring layout data was not migrated; the active proximity-ring timing cue is preserved.

### Phase 6: Delete old UI layout path — done (with one exception)

1. `content/ui/screens/*.json` files have been removed; `content/ui/routes.json` is intentionally kept until routing moves to code or another non-layout source.
2. `ui_loader.*` layout loading code is gone.
3. `ui_layout_cache.h` and all ctx layout cache writes/reads are gone.
4. `ui_button_spawner.h` is gone; migrated screens do not depend on widget entities/hitboxes.
5. `UIElementTag` / `UIText` / `UIButton` / `UIShape` components are gone.
6. Legacy hit-test helpers are gone; migrated controls rely on raygui/controller callbacks and semantic events rather than UI hitbox entities.

## Validation

- `cmake --build build` and `./build/shapeshifter_tests` pass with zero warnings (`-Wall -Wextra -Werror`, `/W4 /WX`).
- Native unity build still catches ODR hazards in hand-written code; the `RAYGUI_IMPLEMENTATION` TU is excluded from unity batching.
- WASM build remains warning-free and does not unity-merge raygui implementation or generated C.
- No runtime path opens `content/ui/screens/*.json` (those files are gone).
- No project-owned `HudLayout`, `LevelSelectLayout`, or `OverlayLayout` cache struct exists; the only matches under `app/` are the generated `*LayoutState` structs that are allowed to live inside `app/ui/generated/`.
- No stale JSON `approach_ring` data is referenced by UI code; active proximity-ring rendering is owned by the gameplay HUD screen controller.
- Screen behavior parity is verified for title, level select, pause/resume, game over, song complete, settings, and gameplay HUD controls.

## Open issues / risks

1. Excalidraw is unavailable in this session. If a visual source exists, export or restore it before final visual polish.
2. Dynamic text/control slots may require a consistent rguilayout naming convention so generated headers expose stable symbols.
3. Settings is present in `routes.json` only via `GamePhase::Settings` / route support; verify route/state coverage as routes evolve.

## Contributing agent inputs

This assembled spec incorporates:

- Redfoot: UI screen inventory and JSON/current-flow analysis.
- Keaton: C++/ECS boundary analysis, revised to remove ECS layout mirrors.
- Hockney: rguilayout vendor path, generated-file placement, and deferred CMake/CI/build integration guidance.
- Keyser: architecture spine and corrected layout-data boundary.
