# raygui + rguilayout UI Migration Spec

## Status

Draft for `ui_layout_refactor`.

This spec replaces the current JSON-driven UI layout path with raygui controls and rguilayout-authored/generated layout files. It is based on the existing `content/ui/screens/*.json`, `content/ui/routes.json`, `design-docs/game-flow.md`, and the current implementation under `app/ui/` and `app/systems/ui_render_system.cpp` (raygui screen-controller dispatch + render-pass glue).

Current directive: the rguilayout application is vendored under `tools/rguilayout/`; `.rgl` files replace the current `content/ui` JSON layout sources; exported `.c/.h` files live directly under `app/ui`; CMake/CI/build-pipeline integration is deferred.

The Excalidraw tool currently returns an empty scene, so Excalidraw is not a usable source of truth for this draft. If Excalidraw is restored later, it should be used as visual reference only; rguilayout `.rgl` files remain the authoring source for eventual compiled UI layout.

## Goals

1. Replace hand-authored runtime JSON UI layout files with rguilayout `.rgl` authoring files and generated `.c/.h` exports. Wiring those generated files into the game build is a later integration task.
2. Use raygui for menu, overlay, settings, pause, and HUD UI controls where appropriate.
3. Keep layout geometry contained in rguilayout exported files. Do not mirror exported rectangles, anchors, widget IDs, or hit targets into ECS components, ECS entities, or `reg.ctx()` layout caches.
4. Preserve existing `GamePhase`-driven screen-controller dispatch and dispatcher-driven game commands.
5. Remove duplicated UI geometry across JSON, `ui_layout_cache.h`, and `ui_button_spawner.h`.

## Non-goals

- Do not change gameplay scoring, energy, input semantics, level selection rules, or song result computation.
- Do not reintroduce approach/proximity rings. They were removed and are out of scope for this migration. Stale `approach_ring` data in `content/ui/screens/gameplay.json` should not be carried forward.
- Do not create custom ECS layout components/entities to represent rguilayout data.
- Do not require rguilayout to be installed in CI or at runtime.
- Do not add CMake targets, CI compile steps, cache changes, or generated-code build wiring in the near-term authoring phases.

## Core decision: exported files own layout data

The rguilayout export is the intended runtime boundary for UI layout data once build integration lands. Until then, generated files may be committed as migration artifacts but are not wired into CMake, CI, or the running game.

The rguilayout tool itself lives in:

```text
tools/rguilayout/
```

For each migrated screen, commit:

| File | Role | Runtime behavior |
|---|---|---|
| `content/ui/screens/<screen>.rgl` | Editable rguilayout source replacing `content/ui/screens/<screen>.json` | Not compiled |
| `app/ui/<screen>.c` | Generated C export | Committed but not built until the later build-integration task |
| `app/ui/<screen>.h` | Generated API/header | Committed beside the generated C file; included by future adapters after build integration |

Recommendation: keep screen layouts under `content/ui/screens/*.rgl` because the existing split already stores screen JSON in `content/ui/screens/` and keeps `content/ui/routes.json` at the root. Use root-level `content/ui/*.rgl` only if a future non-screen/global layout source is introduced. Route data is not layout geometry; do not replace `routes.json` with an `.rgl` unless routing is moved to another non-layout source.

The generated `.c/.h` files contain the runtime layout geometry and generated `GuiLayout_*`-style APIs. Hand-written code may call generated APIs and may pass dynamic state/text into them after the deferred integration task, but it must not copy generated rectangles into project-owned structs or ECS storage.

Disallowed:

- `RaguiAnchors<ScreenTag>` or similar ECS/ctx anchor structs.
- Rebuilding `HudLayout`, `LevelSelectLayout`, or `OverlayLayout` from rguilayout constants.
- `UIElementTag`-style entities for generated widgets.
- Legacy UI hit-test entities whose only purpose is raygui widget hit testing.
- Hand-written normalized-coordinate tables that duplicate `.rgl` data.

Allowed:

- Existing game/menu state in ECS or ctx: active phase/screen, score, high score, energy, selected song, selected difficulty, settings values, dispatcher/services.
- Thin non-ECS adapters that include generated headers, call generated layout APIs, bind runtime text/state, apply platform guards, and translate raygui return values into existing events.
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
app/ui/<screen>.c/.h
        |
        | deferred: future CMake/CI integration wires generated files
        v
app/ui/rguilayout_adapters/<screen>_adapter.cpp
        |
        | included/called by UI render path
        v
ui_render_system()
        |
        | raygui return values become existing ButtonPressEvent / navigation actions
        v
existing dispatcher + game state systems
```

Screen controllers and `ui_render_system` remain responsible for screen dispatch and widget interaction. They should not load JSON, spawn widget entities, or populate layout caches for rguilayout screens.

`ui_render_system` switches on the active screen and calls the relevant adapter. The adapter calls generated layout functions directly. If the generated header exposes named controls, slots, or rectangles, adapters may use those generated symbols directly; the key rule is that the values stay in generated files and are not persisted into ECS or copied into hand-written layout tables.

## Screen migration scope

| Screen | Migration target | Notes |
|---|---|---|
| Title | Generated layout + raygui adapter | Preserve start/confirm behavior. Exit stays platform-gated off web. |
| Tutorial | Generated layout + adapter | Runtime demo shapes may be drawn by custom functions into generated slots. |
| Level Select | Generated layout + adapter | Song list may remain data-driven, but card placement must come from generated layout or generated list slots, not `LevelSelectLayout`. |
| Gameplay HUD | Generated layout + adapter/custom controls | HUD is in scope. Use generated placement for score, high score, pause, energy, lane divider, and shape buttons. |
| Paused | Generated layout + adapter | Preserve dim overlay behavior; overlay geometry/color must not come from JSON caches. |
| Game Over | Generated layout + adapter | Dynamic score/high score/death reason is bound at render time. |
| Song Complete | Generated layout + adapter | Dynamic score/high score/stat table is bound at render time. |
| Settings | Generated layout + adapter | Wire only when `GamePhase::Settings` / route support exists; runtime toggle text is state-derived. |

## HUD treatment

The HUD is part of this refactor. The only thing out of scope is removed approach/proximity-ring behavior.

HUD controls should be implemented as raygui or raygui-style immediate-mode functions, with layout placement supplied by generated rguilayout exports:

| HUD element | Implementation |
|---|---|
| Score / high score | `GuiLabel` or existing text draw called from the HUD adapter using generated slots. |
| Pause button | Stock `GuiButton` is acceptable. |
| Energy bar | `GuiProgressBar` or project `GuiEnergyBar(...)` custom control. LOW blink/text/border behavior stays dynamic, but bounds come from generated layout. |
| Shape buttons | Project `GuiShapeButton(...)` custom controls are acceptable for circular hit testing, colorblind glyphs, and shape-specific drawing. Placement comes from generated layout. |
| Lane divider | Custom draw using generated layout position. |
| Approach rings | Out of scope; do not port stale JSON `approach_ring` data. |

Custom controls are not ECS entities. They are immediate-mode UI functions called during rendering. They may read game state and return clicks/commands, but they do not own persistent layout data.

## Adapter boundary

Adapters live under:

```text
app/ui/rguilayout_adapters/
  title_adapter.cpp/.h
  tutorial_adapter.cpp/.h
  level_select_adapter.cpp/.h
  gameplay_hud_adapter.cpp/.h
  paused_adapter.cpp/.h
  game_over_adapter.cpp/.h
  song_complete_adapter.cpp/.h
  settings_adapter.cpp/.h
```

A single `app/ui/rguilayout_adapters.cpp` bootstrap file is acceptable only for the first screen; split once a second screen lands.

Adapters may contain:

- `extern "C"` / C++ include boundaries for generated headers.
- Calls to generated `GuiLayout_*` functions or generated control symbols.
- Runtime text binding via existing state/resolvers.
- Platform guards such as web-only/desktop-only visibility.
- Viewport transform setup/reset if generated coordinates need virtual-screen mapping.
- Dispatcher/event glue translating UI clicks into existing commands.
- Short-lived stack-local state required by raygui/generated APIs.

Adapters must not contain:

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

The adapter reads existing game state/resolvers and passes current values into generated layout calls or uses generated slots to draw dynamic controls. This preserves dynamic behavior without copying layout data into ECS.

If rguilayout's generated API cannot directly accept a dynamic string for a control, prefer one of these approaches:

1. Author the control as a named empty/placeholder slot in `.rgl`, expose it in generated output, and draw the dynamic raygui/custom control from the adapter using that generated slot.
2. Adjust the `.rgl`/export pattern so the generated file exposes a generated hook/state struct for dynamic labels.

Do not solve this by creating project-owned layout constants or ECS layout mirrors.

## Input and event flow

raygui controls perform immediate-mode hit testing. The adapter converts successful clicks/activations into the same semantic events the game already understands.

Examples:

- Title start -> existing confirm/start event path.
- Pause resume/menu -> existing pause/menu actions.
- Level select difficulty/start -> existing level-selection state/actions.
- HUD pause -> existing pause action.
- Shape button click/touch -> existing player shape-change path.

For migrated raygui controls, do not spawn parallel ECS hit-test entities. Generated adapters should emit semantic UI/gameplay actions directly as they take ownership of those controls.

## Build integration

Deferred. Do not add a CMake target for rguilayout output in the near-term authoring phases, and do not require CI to compile generated `.c/.h` files yet.

For now:

- `tools/rguilayout/` is the vendored application/tooling location.
- `.rgl` layout sources live in `content/ui/screens/` unless a future global/root layout source is justified.
- Generated `.c/.h` exports live directly in `app/ui/`.
- Generated files are committed artifacts only; they are not part of `shapeshifter`, `shapeshifter_tests`, native CI, or WASM CI until a later build-integration task.

Future build integration should decide the exact raygui implementation target shape, generated-C target shape, unity exclusions, include directories, warning policy, and native/WASM CI coverage. When that task happens, generated code should be isolated from unity batching and kept warning-clean without weakening warnings for hand-written code.

## CI and cache

- CI does not run rguilayout.
- CI does not compile committed generated `.c/.h` files until the deferred build-integration task.
- No CMake cache-key changes are required for the near-term authoring/documentation phases.
- In the future PR that wires raygui/generated layout targets into CMake, update native and WASM cache keys for any new build inputs.
- No WASM preload changes are needed unless a later design introduces external UI assets. This spec does not require an external `.rgs` style file.

## Migration phases

### Phase 1: Authoring source layout

1. Use the vendored rguilayout application under `tools/rguilayout/`.
2. Store screen `.rgl` files as `content/ui/screens/<screen>.rgl`, replacing the matching `content/ui/screens/<screen>.json` layout source when that screen migrates.
3. Keep `content/ui/routes.json` until routing is moved to code or another non-layout source.
4. Export generated `.c/.h` files directly to `app/ui/<screen>.c` and `app/ui/<screen>.h`.
5. Do not wire generated files into CMake, CI, or runtime yet.

### Phase 2: First screen authoring proof

1. Author and export the Title screen: `content/ui/screens/title.rgl` + `app/ui/title.c/.h`.
2. Preserve tap/Enter start behavior and web exit-button gating in the intended adapter contract.
3. Leave the generated files unwired until the deferred build-integration task.

### Phase 3: Deferred build integration

Add raygui/generated-code build support, adapter compilation, native CI coverage, and WASM CI coverage in a dedicated platform task. This is intentionally not required for the near-term rguilayout authoring handoff.

### Phase 4: Menus and overlays

Migrate Tutorial, Paused, Game Over, Song Complete, Level Select, and Settings one at a time. Each screen deletes its JSON/layout-cache/widget-entity dependency when its generated adapter is live.

### Phase 5: Gameplay HUD

Migrate HUD placement to rguilayout generated exports and adapters. Implement shape buttons and energy bar as raygui-style custom immediate-mode controls if stock raygui controls are insufficient. Do not migrate stale approach-ring data.

### Phase 6: Delete old UI layout path

Once all screens are generated:

1. Delete `content/ui/screens/*.json` and `content/ui/routes.json` if routes have been moved to code or another non-layout source.
2. Delete `ui_loader.*` layout loading code.
3. Delete `ui_layout_cache.h` and all ctx layout cache writes/reads.
4. Keep `ui_button_spawner.h` removed; migrated screens should not depend on widget entities/hitboxes.
5. Delete `UIElementTag`/`UIText`/`UIButton`/`UIShape` components if they no longer serve non-layout gameplay behavior.
6. Keep legacy hit-test helpers removed; migrated controls should rely on raygui/controller callbacks and semantic events rather than UI hitbox entities.

## Validation

- Documentation/source-layout correction does not create `.rgl` files or generated `.c/.h` files.
- Search confirms near-term phases do not require a CMake target or CI job for generated rguilayout output.
- Future native build and tests remain warning-free once generated files are wired.
- Future native unity build still catches ODR hazards in hand-written code.
- Future WASM build remains warning-free and does not unity-merge raygui implementation or generated C.
- No runtime path opens `content/ui/screens/*.json` after full migration.
- Search confirms no `HudLayout`, `LevelSelectLayout`, `OverlayLayout`, or replacement layout-cache structs remain after Phase 6.
- Search confirms no approach-ring data is referenced by migrated UI code.
- Screen behavior parity is verified for title, level select, pause/resume, game over, song complete, settings, and gameplay HUD controls.

## Open issues / risks

1. Excalidraw is unavailable in this session. If a visual source exists, export or restore it before final visual polish.
2. rguilayout's exact generated API shape must be validated with the first exported screen. The no-ECS-layout-mirror rule holds regardless of API shape.
3. Dynamic text/control slots may require a consistent rguilayout naming convention so generated headers expose stable symbols.
4. Settings is present as JSON today but not in `routes.json`; route/state work is required before the settings screen can be fully reachable.
5. Current `content/ui/screens/gameplay.json` contains stale `approach_ring` fields. The migration should intentionally drop them.

## Contributing agent inputs

This assembled spec incorporates:

- Redfoot: UI screen inventory and JSON/current-flow analysis.
- Keaton: C++/ECS boundary analysis, revised to remove ECS layout mirrors.
- Hockney: rguilayout vendor path, generated-file placement, and deferred CMake/CI/build integration guidance.
- Keyser: architecture spine and corrected layout-data boundary.
