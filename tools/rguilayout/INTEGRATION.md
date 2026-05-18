# rguilayout Integration Path

Issue #1193 replaced the broken `rguilayout.app --template` workflow (plus the
manual-extraction step that worked around it) with a small Python parser +
emitter that produces per-screen ECS entity-spawner functions.

## Pipeline

```
content/ui/screens/<screen>.rgl          # source-of-truth (rguilayout.app visual designer)
        ↓
tools/rguilayout/codegen.py              # parses .rgl, emits entity spawners
        ↓
app/systems/generated/<screen>_screen.cpp     # generated, COMMITTED
        ↓
linked into shapeshifter_lib via CMakeLists.txt
```

## Generated shape

Each `.rgl` row becomes one ECS entity carrying:

| Component                  | Source         |
|---------------------------|----------------|
| Per-screen tag (e.g. `SettingsScreenTag`) | `app/tags/tags.h` — identifies which screen owns the entity |
| Per-kind tag (`UiLabelTag` / `UiButtonTag` / `UiDummyRecTag`) | `app/tags/tags.h` — replaces a `UiKind` enum with existential processing |
| `UiPosition { x, y }`     | `app/components/ui.h` |
| `UiBounds   { w, h }`     | `app/components/ui.h` |
| `UiLabel    { text[64] }` | `app/components/ui.h` |
| `OnPress    { action }`   | `app/components/ui.h` — buttons only |

For each `.rgl <screen>.rgl` the codegen emits:

```cpp
void spawn_<screen>_screen(entt::registry& reg);     // creates one entity per row
void despawn_<screen>_screen(entt::registry& reg);   // destroys all entities with the per-screen tag
```

## Adding / editing screens

1. Edit `content/ui/screens/<screen>.rgl` in `tools/rguilayout/rguilayout.app`
   (or by hand — the format is plain text, see `USAGE.md`).
2. If the edit introduces a new button control whose name is not already in
   `app/components/actions.h::ActionId`, add the new enumerator (sorted
   alphabetically; the codegen will print a clear error otherwise).
3. If you add a new screen file, register its stem in
   `tools/rguilayout/codegen.py::SCREEN_TAGS` AND add the matching per-screen
   tag struct to `app/tags/tags.h` AND add the new file to the
   `RGUILAYOUT_RGL_FILES` / `RGUILAYOUT_GENERATED_SOURCES` lists in
   `CMakeLists.txt`.
4. Run `cmake --build build` — the codegen runs automatically when any `.rgl`
   (or the codegen script) is newer than the corresponding `.cpp`.
5. Commit the regenerated `app/systems/generated/<screen>_screen.cpp` along with
   the `.rgl` edit.

You can also invoke the codegen by hand:

```bash
python3 tools/rguilayout/codegen.py \
    --output-dir   app/systems/generated \
    --actions-header app/components/actions.h \
    content/ui/screens/*.rgl
```

It is idempotent — running twice on unchanged inputs produces zero file writes.

## Recognised `.rgl` control type codes

| Code | Meaning  | Maps to        |
|------|----------|----------------|
| 4    | Label    | `UiLabelTag`    |
| 5    | Button   | `UiButtonTag` + `OnPress<ActionId::<name>>` |
| 24   | DummyRec | `UiDummyRecTag` (visual placeholder / icon slot) |

Add new entries to `RGL_TYPES` in `codegen.py` to extend the table.

## Runtime pipeline

Once each spawner is compiled in, the per-screen entity lifecycle is owned
end-to-end by ECS:

- `app/systems/screen_lifecycle_system.{h,cpp}` — converges the spawned entity
  set toward the active `GamePhase*Tag` ctx mirror. One row per screen in
  `kLifecycleRows`.
- `app/systems/ui_render_system.cpp` — `render_ui_entities` iterates the
  entity views directly (`UiLabelTag` / `UiButtonTag` / `UiDummyRecTag`); no
  `GamePhase` switch.
- `app/systems/ui_update_system.cpp` — hit-tests `UiButtonTag` entities
  against the pointer-release event and dispatches `OnPress::action` through
  the per-`ActionId` function-pointer table (`kActionHandlers`).

The single `RAYGUI_IMPLEMENTATION` translation unit lives at
`app/util/raygui_impl.cpp`; the codegen output lives under
`app/systems/generated/`. There is no `app/ui/` folder, no per-screen
OOP controller bridge, and no `*_layout.h` god-struct header — every
control is one entity carrying atomic components.

## Doctrine

Each generated control entity carries atomic components only — no
`*LayoutState` god-struct exists anywhere in the pipeline. Tags replace
discriminator enums per Fabian's existential processing (`.squad/decisions.md`
§ "DoD source-text grounding"). The render and input systems use
`view<UiButtonTag, ScreenTag>()` joins; they never `switch` on a kind or a
screen identifier.
