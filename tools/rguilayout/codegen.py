#!/usr/bin/env python3
"""rguilayout codegen — `.rgl` → per-screen entity-spawner `.cpp` files.

Per issue #1193, this replaces `rguilayout.app`'s broken `--template` flag and
the manual-extraction workflow with a small parser + emitter. Each `.rgl`
becomes one `<screen>_screen.cpp` that exposes:

    void spawn_<screen>_screen(entt::registry& reg);
    void despawn_<screen>_screen(entt::registry& reg);

Each control row in the `.rgl` becomes one ECS entity carrying atomic
components (`UiPosition`, `UiBounds`, `UiLabel`, optionally `OnPress`) plus
two existential tags: a per-screen tag and a per-kind tag. See
`app/components/ui.h`, `app/components/actions.h`, and `app/tags/tags.h`
for the component / tag declarations.

Invocation (driven by CMake):

    python3 tools/rguilayout/codegen.py \\
        --output-dir   app/systems/generated \\
        --actions-header app/components/actions.h \\
        content/ui/screens/title.rgl \\
        content/ui/screens/settings.rgl \\
        ...

`--actions-header` is read in verify mode: codegen aborts with a non-zero
exit code if any button control name in any `.rgl` is missing from the enum.
This keeps the enum in sync with the layouts without making the header itself
codegen-owned.

Output is deterministic — running on the same inputs twice produces
byte-identical `.cpp` files. CMake invokes this script via an
`add_custom_command(OUTPUT …)` whose DEPENDS list includes all `.rgl` files
and the script itself.
"""

from __future__ import annotations

import argparse
import dataclasses
import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Tuple


# ── .rgl type-code table ────────────────────────────────────────────────
# Maps the type code in the .rgl row to a (per-kind tag, needs-OnPress) tuple.
RGL_TYPES: Dict[int, Tuple[str, bool]] = {
    4:  ("UiLabelTag",    False),  # static text
    5:  ("UiButtonTag",   True),   # pressable
    24: ("UiDummyRecTag", False),  # visual placeholder / icon slot
}


# ── Per-control-name extra tags ─────────────────────────────────────────
# Names → list of extra existential tags emitted on the entity alongside
# the per-kind tag. Per Fabian's existential processing (issue #1202/#1204),
# "which flat 2D icon does this slot render?" / "which entities are hidden
# on PLATFORM_WEB?" are tag-presence rows rather than discriminator fields.
#
# Adding a new name → tag mapping: the tag must already exist in
# `app/tags/tags.h`; the codegen does not generate tag declarations.
NAME_EXTRA_TAGS: Dict[str, Tuple[str, ...]] = {
    # Title-screen shape preview icons. The renderer iterates one view per
    # shape tag (see `ui_render_system.cpp`) and calls the matching row of
    # `shape_draw_2d::kFlatDrawFns`.
    "ShapeCircle":   ("UiShapeIconCircleTag",),
    "ShapeSquare":   ("UiShapeIconSquareTag",),
    "ShapeTriangle": ("UiShapeIconTriangleTag",),
    "ShapeHexagon":  ("UiShapeIconHexagonTag",),
    # Title-screen ExitButton is invisible on Web per #511 but still
    # functions as a tap-anywhere dead-zone. `ui_render_system` and
    # `ui_update_system` skip entities carrying this tag under
    # PLATFORM_WEB; `title_start_tap_system` keeps treating their bounds
    # as a dead-zone (defense-in-depth).
    "ExitButton":    ("UiHiddenOnWebTag",),
    # Settings screen toggle buttons (#1295). `UiToggleTag` selects toggle
    # controls; the state itself is existential (`UiToggleOnTag` /
    # `UiToggleOffTag`) and is updated each frame by
    # `settings_screen_bind_system` from `SettingsState`. Both defaults are
    # ON to match `SettingsState{haptics_enabled=true, reduce_motion=false}`.
    "HapticsToggle":      ("UiToggleTag", "UiToggleOnTag"),
    "ReduceMotionToggle": ("UiToggleTag", "UiToggleOnTag"),
    # Gameplay HUD lane buttons (#1297). Each shape button carries the
    # per-shape icon tag (drives the flat 2D icon row inside the lane
    # button render pass) plus `LaneButtonTag` which distinguishes them
    # from the Title-screen shape preview icons (which use only the
    # `UiShapeIcon*Tag` and render via the generic icon pass). The lane
    # button render pass excludes the generic icon pass via the marker.
    "ShapeButtonCircle":   ("LaneButtonTag", "UiShapeIconCircleTag"),
    "ShapeButtonSquare":   ("LaneButtonTag", "UiShapeIconSquareTag"),
    "ShapeButtonTriangle": ("LaneButtonTag", "UiShapeIconTriangleTag"),
}


# Map of screen-file stem → per-screen tag name. The codegen aborts if a
# `.rgl` is passed whose stem isn't in this table — the per-screen tag must
# exist in `app/tags/tags.h` before the codegen will emit a spawner for it.
SCREEN_TAGS: Dict[str, str] = {
    "title":         "TitleScreenTag",
    "level_select":  "LevelSelectScreenTag",
    "tutorial":      "TutorialScreenTag",
    "gameplay":      "GameplayHudTag",
    "paused":        "PausedScreenTag",
    "game_over":     "GameOverScreenTag",
    "song_complete": "SongCompleteScreenTag",
    "settings":      "SettingsScreenTag",
}


@dataclasses.dataclass(frozen=True)
class Anchor:
    id: int
    name: str
    x: float
    y: float
    enabled: bool


@dataclasses.dataclass(frozen=True)
class Control:
    id: int
    type_code: int
    name: str
    x: float
    y: float
    w: float
    h: float
    anchor_id: int
    text: str


@dataclasses.dataclass(frozen=True)
class Layout:
    stem: str           # path stem of the .rgl file
    ref_w: int
    ref_h: int
    anchors: Tuple[Anchor, ...]
    controls: Tuple[Control, ...]


# ── Parser ───────────────────────────────────────────────────────────────

def parse_rgl(path: Path) -> Layout:
    """Parse a single .rgl file. Lines beginning with # are comments."""
    anchors: List[Anchor] = []
    controls: List[Control] = []
    ref_w = 0
    ref_h = 0
    with path.open("r", encoding="utf-8") as fh:
        for raw_line in fh:
            line = raw_line.rstrip("\r\n")
            if not line or line.lstrip().startswith("#"):
                continue
            tokens = line.split(None, 1)
            if not tokens:
                continue
            tag = tokens[0]
            rest = tokens[1] if len(tokens) > 1 else ""
            if tag == "r":
                fields = rest.split()
                if len(fields) != 4:
                    _fail(path, f"`r` row needs 4 fields, got {len(fields)}: {line!r}")
                _, _, w, h = fields
                ref_w = int(w)
                ref_h = int(h)
            elif tag == "a":
                fields = rest.split()
                if len(fields) != 5:
                    _fail(path, f"`a` row needs 5 fields, got {len(fields)}: {line!r}")
                aid, name, x, y, enabled = fields
                anchors.append(Anchor(
                    id=int(aid),
                    name=name,
                    x=float(x),
                    y=float(y),
                    enabled=(enabled != "0"),
                ))
            elif tag == "c":
                # Control row: id type name x y w h anchor_id [text...]
                # `text` is everything after the 8th whitespace-separated
                # field and may contain spaces ("HAPTICS: ON") or be empty.
                m = re.match(
                    r"\s*(\d+)\s+(\d+)\s+(\S+)\s+(-?\d+(?:\.\d+)?)\s+"
                    r"(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s+"
                    r"(-?\d+(?:\.\d+)?)\s+(\d+)(?:\s+(.*))?\s*$",
                    rest,
                )
                if not m:
                    _fail(path, f"unparseable control row: {line!r}")
                cid, ctype, name, cx, cy, cw, ch, anc, text = m.groups()
                controls.append(Control(
                    id=int(cid),
                    type_code=int(ctype),
                    name=name,
                    x=float(cx),
                    y=float(cy),
                    w=float(cw),
                    h=float(ch),
                    anchor_id=int(anc),
                    text=text if text is not None else "",
                ))
            else:
                _fail(path, f"unknown row tag {tag!r}: {line!r}")
    if ref_w <= 0 or ref_h <= 0:
        _fail(path, "missing `r` reference-window row")
    return Layout(
        stem=path.stem,
        ref_w=ref_w,
        ref_h=ref_h,
        anchors=tuple(anchors),
        controls=tuple(controls),
    )


# ── Codegen ──────────────────────────────────────────────────────────────

def _cpp_escape(text: str) -> str:
    """Escape a string for embedding in a C++ `"..."` literal."""
    return (
        text
        .replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )


def _anchor_offset(layout: Layout, anchor_id: int, path: Path) -> Tuple[float, float]:
    for a in layout.anchors:
        if a.id == anchor_id:
            return (a.x, a.y)
    _fail(path, f"control references unknown anchor id {anchor_id}")
    raise SystemExit(2)  # unreachable; appeases type-checkers


def emit_spawner_cpp(layout: Layout, path: Path) -> str:
    if layout.stem not in SCREEN_TAGS:
        _fail(path, f"no per-screen tag registered for stem {layout.stem!r} "
                    f"in SCREEN_TAGS — update SCREEN_TAGS and add the tag to "
                    f"`app/tags/tags.h` before regenerating")
    screen_tag = SCREEN_TAGS[layout.stem]

    out: List[str] = []
    a = out.append
    a("// Auto-generated by tools/rguilayout/codegen.py — DO NOT EDIT.")
    a(f"// Source: content/ui/screens/{layout.stem}.rgl")
    a("// Each control row in the .rgl becomes one ECS entity carrying a")
    a("// per-screen tag + per-kind tag + atomic UI components.")
    a("// Regenerate via `cmake --build` (codegen runs on .rgl change).")
    a("")
    a('#include "components/actions.h"')
    a('#include "components/ui.h"')
    a('#include "tags/tags.h"')
    a("")
    a("#include <entt/entt.hpp>")
    a("")
    a(f"// Reference window from the .rgl: {layout.ref_w}x{layout.ref_h} px.")
    a(f"void spawn_{layout.stem}_screen(entt::registry& reg) {{")
    for c in layout.controls:
        kind = RGL_TYPES.get(c.type_code)
        if kind is None:
            _fail(path, f"control {c.name!r} uses unknown type code "
                        f"{c.type_code} (known: {sorted(RGL_TYPES)})")
        kind_tag, needs_onpress = kind
        anc_x, anc_y = _anchor_offset(layout, c.anchor_id, path)
        # Absolute (anchor + relative) pixel offset, baked at codegen time.
        abs_x = anc_x + c.x
        abs_y = anc_y + c.y
        a(f"    {{")
        a(f"        // {c.name} ({_kind_label(c.type_code)})")
        a(f"        auto e = reg.create();")
        a(f"        reg.emplace<{screen_tag}>(e);")
        a(f"        reg.emplace<{kind_tag}>(e);")
        for extra_tag in NAME_EXTRA_TAGS.get(c.name, ()):
            a(f"        reg.emplace<{extra_tag}>(e);")
        a(f"        reg.emplace<UiPosition>(e, {_fmt_float(abs_x)}, {_fmt_float(abs_y)});")
        a(f"        reg.emplace<UiBounds>(e, {_fmt_float(c.w)}, {_fmt_float(c.h)});")
        a(f"        auto& label = reg.emplace<UiLabel>(e);")
        if c.text:
            a(f'        ui_label_set(label, "{_cpp_escape(c.text)}");')
        else:
            a(f"        (void)label;  // dynamic-text slot; bound by render system")
        if needs_onpress:
            a(f"        reg.emplace<OnPress>(e, ActionId::{c.name});")
        a(f"    }}")
    a("}")
    a("")
    a(f"void despawn_{layout.stem}_screen(entt::registry& reg) {{")
    a(f"    auto v = reg.view<{screen_tag}>();")
    a(f"    reg.destroy(v.begin(), v.end());")
    a("}")
    a("")  # trailing newline
    return "\n".join(out)


def _kind_label(type_code: int) -> str:
    return {4: "label", 5: "button", 24: "dummy rec"}.get(type_code, f"type {type_code}")


def _fmt_float(v: float) -> str:
    """Stable float formatting — integer-valued floats render as `N.0f`."""
    if v == int(v):
        return f"{int(v)}.0f"
    return f"{v:.6g}f"


# ── Action enum verify pass ──────────────────────────────────────────────

_ENUM_BLOCK_RE = re.compile(
    r"enum\s+class\s+ActionId\s*:\s*\w+\s*\{([^}]*)\}\s*;",
    re.MULTILINE | re.DOTALL,
)
_ENUMERATOR_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*(?:=[^,]*)?,?\s*(?://.*)?$")


def read_action_ids(header: Path) -> List[str]:
    text = header.read_text(encoding="utf-8")
    m = _ENUM_BLOCK_RE.search(text)
    if not m:
        _fail(header, "ActionId enum class not found")
    names: List[str] = []
    for line in m.group(1).splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("//"):
            continue
        em = _ENUMERATOR_RE.match(stripped)
        if em:
            names.append(em.group(1))
    return names


def verify_actions(layouts: List[Layout], header: Path) -> None:
    enum_names = set(read_action_ids(header))
    missing: List[str] = []
    seen: set[str] = set()
    for layout in layouts:
        for c in layout.controls:
            if c.type_code == 5 and c.name not in enum_names and c.name not in seen:
                missing.append(f"  {layout.stem}.rgl: {c.name}")
                seen.add(c.name)
    if missing:
        sys.stderr.write(
            "ActionId verify failed — the following button controls have no\n"
            f"matching enumerator in {header}:\n"
            + "\n".join(missing)
            + "\nAdd the missing enumerators (sorted alphabetically) and rerun.\n"
        )
        raise SystemExit(2)


# ── Entry point ──────────────────────────────────────────────────────────

def _fail(path: Path, msg: str) -> None:
    sys.stderr.write(f"{path}: error: {msg}\n")
    raise SystemExit(2)


def main(argv: List[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", required=True, type=Path,
                        help="Directory to write <stem>_screen.cpp into.")
    parser.add_argument("--actions-header", required=True, type=Path,
                        help="Path to app/components/actions.h (verified, not "
                             "rewritten).")
    parser.add_argument("rgl_files", nargs="+", type=Path,
                        help=".rgl files to process.")
    args = parser.parse_args(argv)

    args.output_dir.mkdir(parents=True, exist_ok=True)

    layouts = [parse_rgl(p) for p in args.rgl_files]
    verify_actions(layouts, args.actions_header)

    for layout, path in zip(layouts, args.rgl_files):
        out_path = args.output_dir / f"{layout.stem}_screen.cpp"
        new_text = emit_spawner_cpp(layout, path)
        # Write only when the content changes so that ninja doesn't rebuild
        # downstream targets on every codegen invocation.
        if out_path.exists():
            existing = out_path.read_text(encoding="utf-8")
            if existing == new_text:
                continue
        out_path.write_text(new_text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
