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
app/ui/generated/<screen>_screen.cpp     # generated, COMMITTED
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
5. Commit the regenerated `app/ui/generated/<screen>_screen.cpp` along with
   the `.rgl` edit.

You can also invoke the codegen by hand:

```bash
python3 tools/rguilayout/codegen.py \
    --output-dir   app/ui/generated \
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

## Coexistence with the legacy `*_layout.h` pipeline

The codegen change is the foundation for a larger ECS refactor of the UI
layer. Until the follow-up issues land, the new spawner functions coexist
with:

- `app/ui/generated/*_layout.h` — legacy god-struct headers (manually-extracted).
- `app/ui/screen_controllers/*` — legacy OOP bridges consuming the headers.
- `app/systems/ui_render_system.cpp` — legacy render path dispatching on `GamePhase`.

These will be replaced by an entity-driven render / input system in follow-up
work; see #1193 "Out of scope" for the full list. No screen entity is spawned
or consumed at runtime today — the generated `.cpp` files are compiled to keep
codegen drift visible.

## Doctrine

Each generated control entity carries atomic components only — no
`*LayoutState` god-struct exists anywhere in the new pipeline. Tags replace
discriminator enums per Fabian's existential processing (`.squad/decisions.md`
§ "DoD source-text grounding"). The render and input systems will use
`view<UiButtonTag, ScreenTag>()` joins; they will never `switch` on a
kind or a screen identifier.
