#!/usr/bin/env python3
"""
check_enum_allowlist.py
=======================
Drift guard for issue #1204: prevent new control-flow enums from sneaking
into ``app/``. Every ``enum`` / ``enum class`` declared under ``app/`` must
appear in ``app/.allowed-enums.txt``.

The allowlist file may contain blank lines and ``#`` comments (full-line or
inline). Each remaining non-empty token is a single enum name.

Failure modes (any of these is a non-zero exit):

  * **unlisted** — an enum exists in ``app/`` but is not allowlisted. Either
    eradicate it (per #1202 / #1204 mechanic) or add it with a justification.
  * **stale**    — an allowlist entry no longer matches any enum in ``app/``.
    Delete the stale line; the migration already landed.

Per Fabian's *Existential Processing* (DoD), enums survive in this codebase
only as labels / lookup keys / finite return codes — anything that drives
``switch`` or virtual dispatch must be modeled as per-case component tables.
See ``.squad/decisions.md`` § *DoD source-text grounding (Fabian) Principle 1*.

Exits 0 on success, 1 on failure.

Usage::

    python3 tools/check_enum_allowlist.py
    python3 tools/check_enum_allowlist.py --app-dir app \\
                                          --allowlist app/.allowed-enums.txt
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEFAULT_APP = REPO / "app"
DEFAULT_ALLOWLIST = REPO / "app" / ".allowed-enums.txt"

CXX_SUFFIXES = (".h", ".hpp", ".cpp", ".cc", ".cxx")

ENUM_RE = re.compile(r"^\s*enum(?:\s+class)?\s+(\w+)\b")


def find_enums(app_dir: Path, repo_root: Path | None = None) -> dict[str, list[str]]:
    """Return ``{enum_name: ["relpath:line", ...]}`` for every ``enum`` /
    ``enum class`` declaration found in C/C++ source under *app_dir*.

    Matching is line-anchored: an enum line must have ``enum`` (optionally
    ``enum class``) as its first non-whitespace token, so commented-out or
    string-literal occurrences of the keyword are ignored.

    Paths are reported relative to *repo_root* (default: parent of *app_dir*)
    so the error message survives being copy-pasted out of CI logs.
    """
    repo_root = repo_root if repo_root is not None else app_dir.parent
    out: dict[str, list[str]] = {}
    for path in sorted(app_dir.rglob("*")):
        if path.suffix not in CXX_SUFFIXES or not path.is_file():
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        try:
            rel = path.relative_to(repo_root).as_posix()
        except ValueError:
            rel = path.as_posix()
        for lineno, line in enumerate(text.splitlines(), start=1):
            m = ENUM_RE.match(line)
            if not m:
                continue
            out.setdefault(m.group(1), []).append(f"{rel}:{lineno}")
    return out


def load_allowlist(path: Path) -> set[str]:
    """Parse the allowlist file. Each non-empty, non-comment line is an
    enum name (inline ``# comment`` is also stripped).
    """
    if not path.exists():
        return set()
    names: set[str] = set()
    for raw in path.read_text(encoding="utf-8").splitlines():
        token = raw.split("#", 1)[0].strip()
        if not token:
            continue
        names.add(token)
    return names


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Fail if any enum in app/ is missing from app/.allowed-enums.txt (issue #1204)."
    )
    parser.add_argument("--app-dir", type=Path, default=DEFAULT_APP,
                        help="Root of game source (default: %(default)s)")
    parser.add_argument("--allowlist", type=Path, default=DEFAULT_ALLOWLIST,
                        help="Path to allowlist file (default: %(default)s)")
    args = parser.parse_args(argv)

    if not args.app_dir.is_dir():
        print(f"error: app dir not found: {args.app_dir}", file=sys.stderr)
        return 2

    found = find_enums(args.app_dir)
    allowed = load_allowlist(args.allowlist)

    unlisted = sorted(set(found) - allowed)
    stale = sorted(allowed - set(found))

    if not unlisted and not stale:
        print(f"ok: {len(found)} enum(s) in {args.app_dir.name}/, all allowlisted")
        return 0

    print("error: enum allowlist drift detected (issue #1204)")
    print(f"  allowlist: {args.allowlist}")
    if unlisted:
        print()
        print("  enums declared in app/ but NOT in the allowlist:")
        for name in unlisted:
            sites = ", ".join(found[name])
            print(f"    - {name}  ({sites})")
        print()
        print("  Action: either")
        print("    (a) eradicate the enum via the #1202 / #1204 mechanic")
        print("        (per-case tags or component tables), or")
        print(f"    (b) add the enum name to {args.allowlist.name} with a")
        print("        one-line justification (lookup key / label / keybinding /")
        print("        finite return code per Fabian's Keep examples).")
    if stale:
        print()
        print("  allowlisted enums no longer present in app/ (delete the line):")
        for name in stale:
            print(f"    - {name}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
