#!/usr/bin/env python3
"""Unit tests for tools/check_enum_allowlist.py (issue #1204)."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_enum_allowlist as mod  # noqa: E402


def _write_tree(root: Path, files: dict[str, str]) -> None:
    for rel, body in files.items():
        p = root / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(body, encoding="utf-8")


class TestFindEnums(unittest.TestCase):
    def test_finds_enum_class_with_underlying_type(self) -> None:
        with TemporaryDirectory() as td:
            root = Path(td)
            _write_tree(root, {"app/a.h": "enum class Foo : uint8_t { A, B };"})
            self.assertIn("Foo", mod.find_enums(root / "app", root))

    def test_finds_plain_unscoped_enum(self) -> None:
        with TemporaryDirectory() as td:
            root = Path(td)
            _write_tree(root, {"app/a.h": "enum Bar { A, B };"})
            self.assertIn("Bar", mod.find_enums(root / "app", root))

    def test_ignores_non_cxx_files(self) -> None:
        with TemporaryDirectory() as td:
            root = Path(td)
            _write_tree(root, {
                "app/a.txt": "enum class Ignored1 {};",
                "app/b.md":  "enum class Ignored2 {};",
                "app/c.py":  "enum class Ignored3 {};",
            })
            self.assertEqual(mod.find_enums(root / "app", root), {})

    def test_records_relative_file_line_sites(self) -> None:
        with TemporaryDirectory() as td:
            root = Path(td)
            _write_tree(root, {"app/sub/a.h": "\n\nenum class Foo : int { A };\n"})
            found = mod.find_enums(root / "app", root)
            self.assertEqual(found["Foo"], ["app/sub/a.h:3"])

    def test_finds_multiple_enums_in_one_file(self) -> None:
        with TemporaryDirectory() as td:
            root = Path(td)
            _write_tree(root, {
                "app/a.h": (
                    "enum class A : uint8_t { X };\n"
                    "enum class B : uint8_t { Y };\n"
                ),
            })
            found = mod.find_enums(root / "app", root)
            self.assertEqual(set(found), {"A", "B"})

    def test_does_not_match_enum_in_comment_only_line(self) -> None:
        # The regex anchors on leading whitespace + `enum` keyword. Any line
        # whose first non-whitespace token is `enum` matches; everything else
        # (including comments and string literals) does not.
        with TemporaryDirectory() as td:
            root = Path(td)
            _write_tree(root, {
                "app/a.h": '// enum class Hidden {};\nconst char* s = "enum class Also {};";\n',
            })
            self.assertEqual(mod.find_enums(root / "app", root), {})


class TestLoadAllowlist(unittest.TestCase):
    def test_strips_full_line_and_inline_comments(self) -> None:
        with TemporaryDirectory() as td:
            p = Path(td) / "list.txt"
            p.write_text(
                "# full-line comment\n"
                "\n"
                "Foo  # inline reason\n"
                "Bar\n"
                "   Baz   \n"
                "\n",
                encoding="utf-8",
            )
            self.assertEqual(mod.load_allowlist(p), {"Foo", "Bar", "Baz"})

    def test_missing_file_returns_empty_set(self) -> None:
        self.assertEqual(mod.load_allowlist(Path("/no/such/file.txt")), set())


class TestEndToEnd(unittest.TestCase):
    def _run(self, files: dict[str, str], allowlist: str) -> int:
        with TemporaryDirectory() as td:
            root = Path(td)
            _write_tree(root, files)
            (root / "app").mkdir(exist_ok=True)
            (root / "app" / ".allowed-enums.txt").write_text(allowlist, encoding="utf-8")
            return mod.main([
                "--app-dir", str(root / "app"),
                "--allowlist", str(root / "app" / ".allowed-enums.txt"),
            ])

    def test_exits_zero_when_aligned(self) -> None:
        rc = self._run({"app/a.h": "enum class Foo {};"}, "Foo\n")
        self.assertEqual(rc, 0)

    def test_exits_zero_with_no_enums_and_empty_allowlist(self) -> None:
        rc = self._run({"app/a.h": "// nothing here\n"}, "# empty\n")
        self.assertEqual(rc, 0)

    def test_exits_nonzero_on_unlisted_enum(self) -> None:
        rc = self._run({"app/a.h": "enum class Sneaky {};"}, "# nothing here\n")
        self.assertEqual(rc, 1)

    def test_exits_nonzero_on_stale_allowlist_entry(self) -> None:
        rc = self._run({"app/a.h": "// no enums\n"}, "Stale\n")
        self.assertEqual(rc, 1)

    def test_reports_both_unlisted_and_stale_in_one_run(self) -> None:
        rc = self._run({"app/a.h": "enum class New {};"}, "Old\n")
        self.assertEqual(rc, 1)

    def test_exits_two_when_app_dir_missing(self) -> None:
        with TemporaryDirectory() as td:
            allow = Path(td) / "a.txt"
            allow.write_text("", encoding="utf-8")
            rc = mod.main([
                "--app-dir", str(Path(td) / "does_not_exist"),
                "--allowlist", str(allow),
            ])
            self.assertEqual(rc, 2)


class TestRepoBaseline(unittest.TestCase):
    """The shipped allowlist must stay in sync with the live tree — this
    guards against accidentally landing a PR that desyncs them locally."""

    def test_repo_allowlist_matches_repo_enums(self) -> None:
        rc = mod.main([])  # uses repo defaults
        self.assertEqual(rc, 0, "tools/check_enum_allowlist.py reports drift "
                                "against the shipped app/.allowed-enums.txt")


if __name__ == "__main__":
    unittest.main()
