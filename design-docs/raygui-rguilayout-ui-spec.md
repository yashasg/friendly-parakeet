# raygui + rguilayout UI Migration Spec

## Status

Implemented. All migration phases described below have shipped; this spec now documents the as-built rguilayout / raygui entity-spawner UI architecture.

This spec replaced the legacy JSON-driven UI layout path with raygui controls and rguilayout-authored/generated layout files. The current implementation lives under `app/ui/generated/<screen>_screen.cpp` (per-screen spawner functions emitted by `tools/rguilayout/` codegen), `app/systems/screen_lifecycle_system.{h,cpp}` (per-tag spawn/despawn dispatch), `app/systems/ui_render_system.cpp` (single render pass over `UiLabelTag` / `UiButtonTag` / `UiDummyRecTag` entities), and `app/systems/ui_update_system.cpp` (button-action dispatch). An intermediate per-screen render-callback folder used during migration was deleted in #1308 once #1287 (entity-driven UI) completed.

Current runtime note: proximity rings are active HUD timing feedback driven by `app/systems/approach_ring_envelope_system.cpp` (which writes per-button `ApproachRing` components consumed by `app/systems/ui_render_system.cpp`) and are specified by
`design-docs/rhythm-spec.md`. The migration preserved this behavior; the
historical `approach_ring` JSON layout fields (which lived in the now-removed
`content/ui/screens/gameplay.json`) were intentionally not ported and are not
the active C++ proximity-ring behavior.

Current directive: the rguilayout application is vendored under `tools/rguilayout/`; `.rgl` files have replaced the legacy `content/ui` JSON layout sources; codegen runs at CMake configure time and writes per-screen entity-spawner `.cpp` files plus `screen_spawners.h` declarations under `app/ui/generated/`; the screen spawners are linked into `shapeshifter_lib` and called by `screen_lifecycle_system` whenever the active `GamePhase*Tag` mirror changes.

The Excalidraw tool currently returns an empty scene, so Excalidraw is not a usable source of truth for this spec. If Excalidraw is restored later, it should be used as visual reference only; rguilayout `.rgl` files remain the authoring source for compiled UI layout.

## Goals

1. Replace hand-authored runtime JSON UI layout files with rguilayout `.rgl` authoring files and codegen-emitted entity-spawner sources linked into `shapeshifter_lib`.
2. Use raygui for menu, overlay, settings, pause, and HUD UI controls where appropriate.
3. Keep layout geometry contained in rguilayout exported files. Do not mirror exported rectangles, anchors, widget IDs, or hit targets into hand-written ECS layout caches; the rguilayout codegen IS the entity-spawner contract.
4. Preserve `GamePhase*Tag`-driven screen lifecycle (active tag → matching entity set) and dispatcher-driven game commands (UI button presses → semantic events).
5. Keep UI geometry contained in generated rguilayout exports. The legacy duplicators (`ui_layout_cache.h`, `ui_button_spawner.h`, and the `UIElementTag` / `UIText` / `UIButton` / `UIShape` widget components) have already been removed; do not re-introduce equivalent project-owned layout structures.

## Non-goals

- Do not change gameplay scoring, energy, input semantics, level selection rules, or song result computation.
- Do not re-introduce the stale `approach_ring` layout data that lived in the removed `content/ui/screens/gameplay.json`. Preserve the active C++ proximity-ring HUD behavior unless a separate gameplay/design decision removes it.
- Do not create hand-written ECS layout components/entities to represent rguilayout data; the codegen-emitted spawner is the single producer of UI entities.
- Do not require rguilayout to be installed in CI or at runtime (CI does not run the authoring tool — codegen output is committed).

## Core decision: exported files own layout data

The rguilayout export is the runtime boundary for UI layout data. Generated headers and per-screen entity-spawner sources are committed under `app/ui/generated/` and called by `app/systems/screen_lifecycle_system.cpp`.

The rguilayout tool itself lives in:

```text
tools/rguilayout/
```

For each migrated screen, commit:

| File | Role | Runtime behavior |
|---|---|---|
| `content/ui/screens/<screen>.rgl` | Editable rguilayout source replacing `content/ui/screens/<screen>.json` | Not compiled |
| `app/ui/generated/<screen>_screen.cpp` (e.g. `title_screen.cpp`) | Codegen-emitted C++ source defining `spawn_<screen>_screen(reg)` / `despawn_<screen>_screen(reg)`. Each spawner emplaces one entity per control row from the `.rgl`, carrying atomic `UiPosition` / `UiBounds` / `UiLabel` (+ `OnPress` for buttons) plus a per-screen tag and per-kind tag (`UiLabelTag` / `UiButtonTag` / `UiDummyRecTag`). | Linked into `shapeshifter_lib`; called by `screen_lifecycle_system` when the active `GamePhase*Tag` ctx mirror changes |
| `app/ui/generated/screen_spawners.h` | Forward declarations of every `spawn_*` / `despawn_*` pair emitted by codegen, plus the consuming-system contract comment. | Single header included by `screen_lifecycle_system.cpp` |

The codegen pattern is source-emitting — there are no separate per-screen layout `.h` files. The spawner `.cpp` files compile as ordinary translation units and own their literal rectangles, anchors, and label strings directly.

Recommendation: keep screen layouts under `content/ui/screens/*.rgl` because the existing split already stores screen JSON in `content/ui/screens/` and keeps `content/ui/routes.json` at the root. Use root-level `content/ui/*.rgl` only if a future non-screen/global layout source is introduced. Route data is not layout geometry; do not replace `routes.json` with an `.rgl` unless routing is moved to another non-layout source.

The generated spawners produce the runtime UI entity set — one entity per control. Hand-written code may mutate those entities' atomic components (e.g. `UiLabel.text` for dynamic score text via per-system bind passes such as `gameplay_hud_bind_system`), but it must not create or destroy UI entities outside the spawner contract.

Disallowed:

- `RaguiAnchors<ScreenTag>` or similar hand-written ECS/ctx anchor structs.
- Rebuilding `HudLayout`, `LevelSelectLayout`, or `OverlayLayout` from rguilayout constants.
- Hand-written widget-entity spawners outside `app/ui/generated/`.
- Legacy UI hit-test entities whose only purpose is raygui widget hit testing.
- Hand-written normalized-coordinate tables that duplicate `.rgl` data.

Allowed:

- Existing game/menu state in ECS or ctx: active phase/screen, score, high score, energy, selected song, selected difficulty, settings values, dispatcher/services.
- Per-screen bind systems that read game state and write into the spawner-emitted entities' `UiLabel` / `UiBounds` / per-button colour overrides (see `gameplay_hud_bind_system`, `game_over_scoreboard_bind_system`).
- Custom raygui-style draw functions invoked from `ui_render_system` for non-text controls (e.g. shape buttons, energy bar), provided their placement comes from the spawner-emitted `UiPosition` / `UiBounds` on each entity and not duplicated layout state.

## Runtime architecture

```text
tools/rguilayout/
        |
        | author/edit on developer machine
        v
content/ui/screens/<screen>.rgl
        |
        | codegen.py emits per-screen spawner source
        | (runs at CMake configure time)
        v
app/ui/generated/<screen>_screen.cpp + screen_spawners.h
        |
        | linked into shapeshifter_lib
        v
screen_lifecycle_system (app/systems/screen_lifecycle_system.cpp)
        |
        | invokes spawn_<screen>_screen(reg) when the matching
        | GamePhase*Tag becomes active; despawn_<screen>_screen(reg)
        | when it deactivates
        v
UiLabelTag / UiButtonTag / UiDummyRecTag entities in the registry
        |
        | per-frame
        v
ui_render_system (single render pass over UI entities)
ui_update_system (touch → OnPress dispatch → game commands)
```

The screen-lifecycle table in `screen_lifecycle_system.cpp` maps each `GamePhase*Tag` to one `(spawn, despawn)` row. There is no central `switch (phase)`; convergence runs once per frame and short-circuits when the entity-set membership already matches the active tag.

`ui_render_system` queries `UiLabelTag` / `UiButtonTag` / `UiDummyRecTag` entities directly — it does not know about screens. Per-screen tags (e.g. `PausedScreenTag`) exist on each entity for despawn membership, not for render dispatch. Custom raygui-style controls (shape buttons, energy bar) are emitted as ordinary entities whose draw callback is selected via the per-button kind tag.

## Screen migration scope

| Screen | Migration target | Notes |
|---|---|---|
| Title | Generated entity spawner | Preserve start/confirm behavior. Exit stays platform-gated off web. |
| Tutorial | Generated entity spawner + `tutorial_screen_continue` action handler in `ui_update_system.cpp` | Runtime demo shapes drawn by custom draw callbacks during render. |
| Level Select | Generated entity spawner + `level_select_dynamic_spawn_system` for per-song cards | Song list remains data-driven; static frame comes from `.rgl`, per-card entities are spawned by the dynamic spawn system and carry `LevelSelectScreenTag` so the codegen-emitted `despawn_level_select_screen` reaps them in one query. |
| Gameplay HUD | Generated entity spawner + custom-control draw callbacks | HUD placement comes from generated `UiPosition` / `UiBounds` per entity. Shape buttons and the energy bar are raygui-style custom immediate-mode draw functions selected by per-entity kind tag. |
| Paused | Generated entity spawner | Preserve dim overlay behavior; overlay rectangle comes from `.rgl`. |
| Game Over | Generated entity spawner + `game_over_scoreboard_bind_system` for dynamic score/high score/death-reason text | Bind system mutates `UiLabel.text` per frame on spawner-emitted entities. |
| Song Complete | Generated entity spawner + per-frame bind for stat-table text | Same bind pattern as Game Over. |
| Settings | Generated entity spawner + per-toggle bind for label text and toggle state | Routed via the dedicated `GamePhaseSettingsTag` phase. |

## HUD treatment

The HUD is part of this refactor. Stale JSON `approach_ring` layout fields are
out of scope, but the active proximity-ring timing cue is in scope for behavior
parity and should remain driven by gameplay state.

HUD controls should be implemented as raygui or raygui-style immediate-mode draw functions invoked from `ui_render_system`, with layout placement supplied by the spawner-emitted `UiPosition` / `UiBounds` on each entity:

| HUD element | Implementation |
|---|---|
| Score / high score | `UiLabelTag` entity with `UiLabel.text` written per frame by `gameplay_hud_bind_system`. |
| Pause button | `UiButtonTag` entity with `OnPress` carrying the action that enqueues `NextPhasePausedTag`. |
| Energy bar | `EnergyBarTag` entity; custom raygui-style draw selected by tag in `ui_render_system`. LOW blink/text/border behavior stays dynamic, but bounds come from the spawner-emitted `UiBounds`. |
| Shape buttons | `LaneButtonTag` entities. Custom raygui-style draw selected by tag in `ui_render_system` handles circular hit testing, colorblind glyphs, and shape-specific drawing. Placement comes from the spawner-emitted `UiPosition` / `UiBounds`. |
| Lane divider | `UiDummyRecTag` entity; rectangle from `.rgl`. |
| Proximity rings | Per-`LaneButtonTag` `ApproachRing` component, written every frame by `approach_ring_envelope_system` (which uses `gameplay_hud_ring_cue()` from `app/util/gameplay_hud_ring.{h,cpp}`); rendered as the active timing-cue overlay. Stale JSON `approach_ring` layout data is not used. |

Custom controls are not lifecycle-owning systems. They are immediate-mode draw functions called during the single `ui_render_system` pass, selected by the entity's per-kind tag. They may read game state and call into the dispatcher, but they do not own persistent layout data.

## Producer / consumer boundary

The UI runtime has three responsibility layers; each consumes the previous one:

```text
app/ui/generated/<screen>_screen.cpp + screen_spawners.h   (Producer)
   - spawn_<screen>_screen(reg)   → emplace UI entities for one screen
   - despawn_<screen>_screen(reg) → destroy every entity carrying <screen>Tag

app/systems/screen_lifecycle_system.cpp                    (Coordinator)
   - one row per migrated screen mapping GamePhase*Tag → (spawn, despawn)
   - converges entity-set membership against the active phase tag

app/systems/ui_render_system.cpp                           (Renderer)
   - single pass over UiLabelTag / UiButtonTag / UiDummyRecTag /
     LaneButtonTag / EnergyBarTag entities
   - selects custom draw via per-kind tag

app/systems/ui_update_system.cpp                           (Input dispatch)
   - hit-tests touch / mouse against UiButtonTag entities
   - reads OnPress and invokes the bound action function
```

The single producer is the codegen output under `app/ui/generated/`. Hand-written code never creates UI entities outside this contract; bind systems (`gameplay_hud_bind_system`, `game_over_scoreboard_bind_system`, etc.) only **mutate** atomic component data on spawner-emitted entities. An intermediate per-screen render-callback folder was the staging shape during migration and was deleted in #1308 once #1287 (entity-driven UI) shipped.

Spawner sources may contain:

- Literal `Rectangle` / anchor / colour constants exported by codegen from the `.rgl`.
- One `registry.emplace<...>` per control row, with the per-screen tag and per-kind tag attached.
- One `registry.create` per button carrying an `OnPress { action_fn }` payload.

Spawner sources must not contain:

- Hand-written rectangles outside the codegen output (regenerate from `.rgl` instead).
- Game-state reads (those belong in bind systems consuming the spawner-emitted entities).
- Conditional control creation (the spawner is a flat list — visibility is a bind-system or render-pass concern).

Bind systems and custom-draw callbacks may contain:

- Reads of game state / ctx singletons to compute label text, colour overrides, energy bar fill, etc.
- Writes to `UiLabel.text`, per-entity colour override components, `ApproachRing` ratio, etc.
- Platform guards such as web-only / desktop-only visibility (by toggling per-entity visibility components rather than recreating entities).
- Dispatcher/event glue translating `OnPress` activations into the same semantic events the rest of the game consumes.

## Dynamic text and state binding

Dynamic UI values stay runtime-driven via per-screen bind systems:

- Score / high score (`gameplay_hud_bind_system`, `game_over_scoreboard_bind_system`).
- Energy value and LOW cue (`gameplay_hud_bind_system` → energy-bar entity).
- Current shape and selected shape (per-lane-button colour / glyph override).
- Selected song and difficulty (`level_select_dynamic_spawn_system` for the per-card entities; static frame from spawner).
- Game-over reason (`game_over_scoreboard_bind_system`).
- Song-complete timing/stat table (per-system bind).
- Settings toggle labels and audio offset (per-system bind).

The bind system reads game state/resolvers and writes the current values into the spawner-emitted entity's atomic components (`UiLabel.text`, per-button colour override, etc.). This preserves dynamic behavior without copying layout data into hand-written caches.

If a control needs a dynamic value that the codegen-emitted spawner does not already attach a `UiLabel` (or equivalent component) to, the resolution is one of:

1. Author the control as a named slot in `.rgl` so codegen emits the matching `UiLabelTag` / `UiButtonTag` entity with the right per-screen tag.
2. Add the appropriate atomic component to the spawner output via a codegen template change.

Do not solve this by creating hand-written widget entities outside the spawner contract.

## Input and event flow

`ui_update_system` performs per-frame hit testing against `UiButtonTag` entities (using their `UiPosition` / `UiBounds`). On a hit, it reads the entity's `OnPress` and invokes the bound action function. Each action function maps to the same semantic event the rest of the game already understands.

Examples:

- Title start → existing confirm/start event path.
- Pause resume / menu → existing pause / menu actions.
- Level select difficulty / start → existing level-selection state / actions.
- HUD pause → request `NextPhasePausedTag` (consumed by `game_phase_transition`).
- Shape button click / touch → existing player shape-change path (`ShapePress*Event`).

Hit-test entities are exactly the same `UiButtonTag` / `LaneButtonTag` entities the renderer draws — there is no parallel hit-target geometry. Action functions emit semantic UI/gameplay actions directly.

## Build integration

The UI pipeline is wired into `shapeshifter_lib` as follows:

- `tools/rguilayout/` is the vendored authoring/tooling location. `tools/rguilayout/codegen.py` is invoked at CMake configure time to regenerate `app/ui/generated/<screen>_screen.cpp` + `screen_spawners.h` from `content/ui/screens/*.rgl`. The authoring GUI is not run in CI.
- `.rgl` layout sources live in `content/ui/screens/` unless a future global/root layout source is justified.
- Codegen output `.cpp` files under `app/ui/generated/` are added to `shapeshifter_lib` via the `RGUILAYOUT_GENERATED_SOURCES` list in `CMakeLists.txt`.
- `app/ui/raygui_impl.cpp` is the single-TU owner of `RAYGUI_IMPLEMENTATION`. The macro is set via `COMPILE_DEFINITIONS RAYGUI_IMPLEMENTATION` on that source file, and the file is excluded from unity batching (`SKIP_UNITY_BUILD_INCLUSION TRUE`) so the raygui implementation stays isolated.
- All other UI consumer TUs (`ui_render_system.cpp`, `ui_update_system.cpp`, `screen_lifecycle_system.cpp`, the per-screen bind systems) live in `app/systems/` and pull in raygui headers normally without defining the implementation macro.

The native build (`shapeshifter`, `shapeshifter_tests`) and WASM build compile the spawner sources and raygui implementation together and stay warning-clean under `-Wall -Wextra -Werror`.

## CI and cache

- CI does not run the rguilayout authoring application; it consumes the committed codegen output. The codegen Python script runs at CMake configure time to keep generated sources in sync with `.rgl` edits.
- Native CI and WASM CI compile all UI consumer systems and codegen output as part of `shapeshifter_lib`.
- CMake cache keys cover the codegen-output globs via `CONFIGURE_DEPENDS`; adding a new `.rgl` requires a new spawner forward declaration in `screen_spawners.h` (emitted automatically by codegen) and a new row in `screen_lifecycle_system.cpp`'s lifecycle table.
- No WASM preload changes are needed unless a later design introduces external UI assets. This spec does not require an external `.rgs` style file.

## Migration phases

### Phase 1: Authoring source layout — done

1. Use the vendored rguilayout application under `tools/rguilayout/`.
2. Store screen `.rgl` files as `content/ui/screens/<screen>.rgl`, replacing the matching `content/ui/screens/<screen>.json` layout source as each screen migrates.
3. Keep `content/ui/routes.json` until routing is moved to code or another non-layout source.
4. Codegen emits `app/ui/generated/<screen>_screen.cpp` + entries in `screen_spawners.h`.
5. Wire codegen output into `shapeshifter_lib`.

### Phase 2: First screen authoring proof — done

1. Authored Title: `content/ui/screens/title.rgl` → `app/ui/generated/title_screen.cpp`.
2. Tap/Enter start behavior and web exit-button gating are preserved by the codegen-emitted `OnPress` payloads + per-action handlers in `ui_update_system.cpp`.
3. Generated source compiles into `shapeshifter_lib` and renders via `ui_render_system`.

### Phase 3: Build integration — done

raygui codegen build support, spawner compilation, native CI coverage, and WASM CI coverage are in place. `RAYGUI_IMPLEMENTATION` ownership lives on `app/ui/raygui_impl.cpp` and that TU is excluded from unity batching.

### Phase 4: Menus and overlays — done

Tutorial, Paused, Game Over, Song Complete, Level Select, and Settings each have a codegen-emitted spawner under `app/ui/generated/` and a matching row in `screen_lifecycle_system.cpp`. Their JSON / layout-cache / hand-written widget-entity dependencies have been removed.

### Phase 5: Gameplay HUD — done

HUD placement is driven by `app/ui/generated/gameplay_screen.cpp` (entity spawner). Shape buttons (`LaneButtonTag`) and the energy bar (`EnergyBarTag`) are implemented as raygui-style custom immediate-mode draw functions selected by per-kind tag inside `ui_render_system`. Stale approach-ring layout data was not migrated; the active proximity-ring timing cue is preserved via per-`LaneButtonTag` `ApproachRing` components written by `approach_ring_envelope_system`.

### Phase 6: Delete old UI layout path — done (with one exception)

1. `content/ui/screens/*.json` files have been removed; `content/ui/routes.json` is intentionally kept until routing moves to code or another non-layout source.
2. `ui_loader.*` layout loading code is gone.
3. `ui_layout_cache.h` and all ctx layout cache writes/reads are gone.
4. `ui_button_spawner.h` is gone; migrated screens do not depend on widget entities/hitboxes.
5. `UIElementTag` / `UIText` / `UIButton` / `UIShape` components are gone.
6. Legacy hit-test helpers are gone; migrated controls rely on `ui_update_system` hit-testing `UiButtonTag` entities and dispatching their `OnPress` action functions, which emit the same semantic events the rest of the game already consumes.

## Validation

- `cmake --build build` and `./build/shapeshifter_tests` pass with zero warnings (`-Wall -Wextra -Werror`).
- Native unity build still catches ODR hazards in hand-written code; the `RAYGUI_IMPLEMENTATION` TU is excluded from unity batching.
- WASM build remains warning-free and does not unity-merge raygui implementation or generated C.
- No runtime path opens `content/ui/screens/*.json` (those files are gone).
- No project-owned `HudLayout`, `LevelSelectLayout`, or `OverlayLayout` cache struct exists; layout geometry lives only inside the per-screen spawner `.cpp` files under `app/ui/generated/` as literal constants emitted from each `.rgl` source.
- No stale JSON `approach_ring` data is referenced by UI code; active proximity-ring rendering is driven by per-`LaneButtonTag` `ApproachRing` components written by `approach_ring_envelope_system` and consumed by `ui_render_system`.
- Screen behavior parity is verified for title, level select, pause/resume, game over, song complete, settings, and gameplay HUD controls.

## Open issues / risks

1. Excalidraw is unavailable in this session. If a visual source exists, export or restore it before final visual polish.
2. Dynamic text/control slots may require a consistent rguilayout naming convention so generated headers expose stable symbols.
3. Settings is present in `routes.json` only via the `GamePhaseSettingsTag` / route support; verify route/state coverage as routes evolve.

## Contributing agent inputs

This assembled spec incorporates:

- Redfoot: UI screen inventory and JSON/current-flow analysis.
- Keaton: C++/ECS boundary analysis, revised to remove ECS layout mirrors.
- Hockney: rguilayout vendor path, generated-file placement, and deferred CMake/CI/build integration guidance.
- Keyser: architecture spine and corrected layout-data boundary.
