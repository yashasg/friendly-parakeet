#!/usr/bin/env python3
"""Unit tests for tools/rguilayout/codegen.py.

These tests exercise the parser and emitter on real .rgl inputs to catch
regressions in the codegen output format (#1193).
"""

from __future__ import annotations

import os
import re
import sys
import tempfile
import unittest
from pathlib import Path

# Allow `import codegen` from the rguilayout subpackage.
_HERE = Path(__file__).parent
sys.path.insert(0, str(_HERE / "rguilayout"))

import codegen  # noqa: E402


REPO_ROOT = _HERE.parent
RGL_DIR = REPO_ROOT / "content" / "ui" / "screens"
ACTIONS_HEADER = REPO_ROOT / "app" / "components" / "actions.h"


class ParserTests(unittest.TestCase):

    def test_parse_settings_rgl_extracts_reference_window_and_anchors(self):
        layout = codegen.parse_rgl(RGL_DIR / "settings.rgl")
        self.assertEqual(layout.ref_w, 720)
        self.assertEqual(layout.ref_h, 1280)
        self.assertEqual(len(layout.anchors), 1)
        self.assertEqual(layout.anchors[0].name, "Anchor01")

    def test_parse_settings_rgl_extracts_all_controls(self):
        layout = codegen.parse_rgl(RGL_DIR / "settings.rgl")
        # settings.rgl currently has 10 control rows (5 buttons + 5 labels).
        names = [c.name for c in layout.controls]
        self.assertIn("AudioOffsetMinus", names)
        self.assertIn("AudioOffsetPlus", names)
        self.assertIn("HapticsToggle", names)
        self.assertIn("CloseButton", names)
        # Button control rows are type code 5; verify HapticsToggle was tagged.
        haptics = next(c for c in layout.controls if c.name == "HapticsToggle")
        self.assertEqual(haptics.type_code, 5)
        # Spaces in label text must survive the parser.
        self.assertEqual(haptics.text, "HAPTICS: ON")

    def test_parse_title_rgl_preserves_icon_syntax(self):
        layout = codegen.parse_rgl(RGL_DIR / "title.rgl")
        settings_btn = next(c for c in layout.controls if c.name == "SettingsButton")
        self.assertEqual(settings_btn.text, "#142#")

    def test_parse_rejects_unknown_row_tag(self):
        with tempfile.NamedTemporaryFile("w", suffix=".rgl", delete=False) as fh:
            fh.write("# header\nr 0 0 100 100\nz bogus\n")
            path = Path(fh.name)
        try:
            with self.assertRaises(SystemExit):
                codegen.parse_rgl(path)
        finally:
            os.unlink(path)


class EmitterTests(unittest.TestCase):

    def test_emit_includes_screen_and_kind_tags(self):
        layout = codegen.parse_rgl(RGL_DIR / "settings.rgl")
        out = codegen.emit_spawner_cpp(layout, RGL_DIR / "settings.rgl")
        self.assertIn("spawn_settings_screen", out)
        self.assertIn("despawn_settings_screen", out)
        self.assertIn("SettingsScreenTag", out)
        self.assertIn("UiButtonTag", out)
        self.assertIn("UiLabelTag", out)
        self.assertIn("OnPress", out)
        self.assertIn("ActionId::HapticsToggle", out)

    def test_emit_does_not_use_uikind_enum_or_god_struct(self):
        # Doctrinal: existential per-kind tags replace the spec's UiKind enum.
        # The generated cpp must not reference any *LayoutState god-struct.
        for rgl in sorted(RGL_DIR.glob("*.rgl")):
            layout = codegen.parse_rgl(rgl)
            out = codegen.emit_spawner_cpp(layout, rgl)
            self.assertNotIn("UiKind::", out, f"{rgl} introduced UiKind enum")
            self.assertNotIn("LayoutState", out, f"{rgl} mentions LayoutState")

    def test_emit_attaches_toggle_tag_to_settings_toggles(self):
        # Settings migration (#1295): HapticsToggle and ReduceMotionToggle
        # must carry UiToggleTag so ui_render_system applies the two-cue
        # ON/OFF style.
        layout = codegen.parse_rgl(RGL_DIR / "settings.rgl")
        out = codegen.emit_spawner_cpp(layout, RGL_DIR / "settings.rgl")
        self.assertRegex(
            out,
            re.compile(
                r"// HapticsToggle.*?UiToggleTag",
                re.DOTALL,
            ),
        )
        self.assertRegex(
            out,
            re.compile(
                r"// ReduceMotionToggle.*?UiToggleTag",
                re.DOTALL,
            ),
        )
        # Non-toggle Settings buttons (AudioOffsetMinus / AudioOffsetPlus /
        # CloseButton) must NOT receive UiToggleTag.
        for name in ("AudioOffsetMinus", "AudioOffsetPlus", "CloseButton"):
            block_re = re.compile(
                r"// " + name + r".*?(?=\n\s*\{|\Z)",
                re.DOTALL,
            )
            m = block_re.search(out)
            self.assertIsNotNone(m, f"{name} block not found")
            self.assertNotIn("UiToggleTag", m.group(0),
                             f"{name} should not carry UiToggleTag")

    def test_emit_is_deterministic(self):
        layout = codegen.parse_rgl(RGL_DIR / "settings.rgl")
        first = codegen.emit_spawner_cpp(layout, RGL_DIR / "settings.rgl")
        second = codegen.emit_spawner_cpp(layout, RGL_DIR / "settings.rgl")
        self.assertEqual(first, second)

    def test_anchor_offset_baked_into_position(self):
        # All current .rgl files use Anchor01 at (0,0) so the absolute and
        # relative positions match. A non-zero anchor should additively shift.
        anchored = codegen.parse_rgl(RGL_DIR / "settings.rgl")
        anchored = codegen.Layout(
            stem=anchored.stem,
            ref_w=anchored.ref_w,
            ref_h=anchored.ref_h,
            anchors=(codegen.Anchor(id=1, name="Anchor01", x=10.0, y=20.0, enabled=True),),
            controls=anchored.controls,
        )
        out = codegen.emit_spawner_cpp(anchored, RGL_DIR / "settings.rgl")
        # SettingsTitle is at .rgl coords (210, 400), anchor offset (10, 20).
        # Use DOTALL to span the newline between the comment and the
        # UiPosition emplace line.
        self.assertRegex(
            out,
            re.compile(
                r"// SettingsTitle.*?UiPosition>\(e,\s*220\.0f,\s*420\.0f\)",
                re.DOTALL,
            ),
        )


class ActionsVerifyTests(unittest.TestCase):

    def test_actions_header_covers_all_buttons(self):
        layouts = [codegen.parse_rgl(p) for p in sorted(RGL_DIR.glob("*.rgl"))]
        # Should not raise.
        codegen.verify_actions(layouts, ACTIONS_HEADER)

    def test_verify_actions_fails_on_missing_enum(self):
        layouts = [codegen.parse_rgl(p) for p in sorted(RGL_DIR.glob("*.rgl"))]
        with tempfile.NamedTemporaryFile(
            "w", suffix=".h", delete=False
        ) as fh:
            # Header missing every button action — verify must fail.
            fh.write(
                "#pragma once\nenum class ActionId : int { None = 0, };\n"
            )
            partial = Path(fh.name)
        try:
            with self.assertRaises(SystemExit):
                codegen.verify_actions(layouts, partial)
        finally:
            os.unlink(partial)


class ScreenTagsTableTests(unittest.TestCase):

    def test_screen_tags_covers_all_rgl_files(self):
        rgl_stems = {p.stem for p in RGL_DIR.glob("*.rgl")}
        self.assertEqual(
            rgl_stems,
            set(codegen.SCREEN_TAGS),
            msg="codegen.SCREEN_TAGS is out of sync with content/ui/screens/*.rgl",
        )


class GeneratedSourceTests(unittest.TestCase):

    def test_committed_outputs_match_codegen(self):
        """Every committed app/ui/generated/<stem>_screen.cpp must equal the
        output of running codegen on the current .rgl source. Prevents drift
        between source and generated files."""
        generated_dir = REPO_ROOT / "app" / "ui" / "generated"
        for rgl in sorted(RGL_DIR.glob("*.rgl")):
            layout = codegen.parse_rgl(rgl)
            expected = codegen.emit_spawner_cpp(layout, rgl)
            cpp = generated_dir / f"{layout.stem}_screen.cpp"
            self.assertTrue(cpp.exists(), f"{cpp} missing — run codegen")
            actual = cpp.read_text(encoding="utf-8")
            self.assertEqual(
                actual, expected,
                msg=(
                    f"{cpp} is stale relative to {rgl}. Run:\n"
                    f"  python3 tools/rguilayout/codegen.py "
                    f"--output-dir app/ui/generated "
                    f"--actions-header app/components/actions.h "
                    f"content/ui/screens/*.rgl"
                ),
            )


if __name__ == "__main__":
    unittest.main()
