# Scribe History

## Post-R16 Protocol Fixes

Scribe-23 committed content to wrong path `.squad/decisions/decisions.md` (863 lines, 417 genuine + 446 duplicated historical). Coordinator recovered in commit `ed493ec`.

Three heuristics added to decisions.md canonical log to prevent recurrence:

1. **Path Discipline**: Verify `.squad/decisions.md` exists before appending. NEVER write to `.squad/decisions/decisions.md`.
2. **Selective Git Add**: Use explicit paths only; never `git add -A`. Rationale: Scribe-19 swept Keaton working tree via `-A`.
3. **Inbox Cleanup**: Delete merged inbox files post-append; verify folder clean.

---

## R15: Content Misfile (Scribe-23)

**Issue:** Wrote round 15 + R7/R10 late-merge content to wrong path `.squad/decisions/decisions.md`.
**Recovery:** Coordinator extracted genuine content (lines 447–863) and appended to canonical `.squad/decisions.md` (now 13,053 lines). Deleted misfile.
**Lesson:** Verify target path exists before appending.

---

## R14: Inbox Cleanup Deficiency (Scribe-22)

**Issue:** Left 4 stale inbox files preserved; commit message claimed 8 deletions (actual 2) and 4 preserved (actual 1 committed + ~4 untracked).
**Root:** Scribe's mental model included untracked filesystem files, but git commit only touches tracked state. Count was inaccurate.
**Lesson:** Thorough grep verification before delete/preserve verdict. Audit against canonical `.squad/decisions.md` for merged status.

---

## R12: Selective Add Deficiency (Scribe-19)

**Issue:** Used `git add -A`; swept Keaton's working-tree edits into Scribe's commit, creating false authorship.
**Lesson:** Explicit paths only. Stage only `.squad/` artifacts.

---

## R16: Inbox Merge Execution

**Files merged:**
- `keaton-r16-position-deletion.md` — 50 lines, Position struct deletion decision + files list + test baseline
- `keyser-r16-r15-migration-audit.md` — 423 lines, r15 audit + r16 WIP catch + module health table + process findings

**Deletions:** Both inbox files removed post-append.
**Verifications:**
- Canonical path verified: `[ -f .squad/decisions.md ] && echo "OK"` → OK
- Staged paths verified: only `.squad/` files
- decisions.md post-merge size: 13,530 lines (pre-archive)

**Archival considered:** Archive folder exists but is empty. No archival triggered (post-merge size 13,530 < 14K+ threshold; further growth before next archive pass).

---

## Round 17

**R17 corrections applied:**
- F1: surgical edit at decisions.md:13256-13258 and 13404 to correct latent-fix attribution (r17, not r16; Option A: structurally exclusive views).
- F2: added inbox-lifecycle heuristic to decisions.md clarifying that inbox/*.md files are gitignored intentionally and NEVER committed as files.
- Inbox content (keaton-r17 + keyser-r17) appended to canonical decisions.md; both files deleted from working tree.
- Decisions.md size post-merge: 14,293 lines (over 13K threshold; archival deferred pending round-end review).

---
