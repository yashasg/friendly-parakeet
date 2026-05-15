---
configured: true
interval: 10
timeout: 30
description: "My squad work loop"
---

# Squad Work Loop

> ⚠️ Set `configured: true` in the frontmatter above to activate this loop.
> Run with: `squad loop`

## What to do each cycle

1) **Triage issues**
- Sort open issues by `P0 > P1 > P2 > P3`, then recency.
- If unlabeled: `P0` (crash/build/data-loss/blocker), `P1` (major regression), `P2` (normal bug/refactor/docs), `P3` (nice-to-have).
- Close as outdated only if: no repro on `main` + subsystem removed/changed + inactive >30 days. Leave evidence.
- Monitor squad PRs; if CI/CD is green and required checks/reviews pass, **squash-merge into `main`** and delete source branch.

2) **Execute**
- Handle max 3 issues per cycle (max 1 if any `P0`).
- Repro -> minimal fix -> build/tests -> update issue/PR.
- Prefer raylib/EnTT APIs, ECS-first, early-outs, minimal code.
- Use docs from `docs/entt/` and `docs/raylib_cheatsheet.md` before implementing.
- Remove outdated code/comments while fixing issues; keep docs concise and current.

3) **Fabian drift check** (every cycle, after Execute — produces issues for next cycle, do NOT fix here)
- Audit `app/` against ECS canon (`.squad/decisions.md` § "DoD source-text grounding (Fabian)" — Principles 0–4) and the cross-references in #1203 / #1204. File one focused `ecs_refactor` issue per violation with file:line evidence and the violated principle number.
- Checks (run in parallel):
  - **Folder layout doctrine:** Only `app/components/`, `app/entities/`, `app/systems/`, `app/tags/`, `app/util/` are allowed under `app/`. Any other folder is drift (`audio/`, `rendering/`, `input/`, `content/`, `session/`, `ui/` are scheduled for elimination — anything new added under `app/` is drift).
  - **Existential processing (Principle 0):** Behavior must be dispatched by table membership, not by reading a discriminator field. Grep for `switch (` over a component field, `if (x.kind ==`, `virtual`, `override`, `dynamic_cast` in game code. Each hit is a candidate violation of Principle 0 (entity's class IS its set of tables).
  - **Components vs entities distinction:** Files in `app/components/` must be plain data structs — no methods beyond constructors. Files in `app/entities/` are free factory functions that emplace components on a fresh entity, never classes. Grep `app/components/` for member functions; grep `app/entities/` for class definitions.
  - **No hierarchies:** No inheritance, no virtual dispatch, no class hierarchies in game code. Grep for `: public ` (excluding raylib/EnTT/test framework wrappers).
  - **Tags single-header rule:** All tag types live in a single header inside `app/tags/`. Per-tag header files are drift (Principle 2 — tags are zero-column tables; a single declaration site keeps the registry honest).
  - **Enum allowlist (Principle 1 + cyclomatic ratchet):** Run `make check-enum-allowlist` (target installed by #1204). Failure means a new enum was added without an allowlist entry. File the failure verbatim.
  - **Cyclomatic complexity ratchet:** Count `switch` cases + standalone `if`/`else if` branches in `app/`. Compare against the baseline recorded in `.squad/decisions.md` § "Drift baselines". Number must trend down only — any increase = file an issue and update the baseline only after the increase is reverted.
- If multiple violations cluster on one file, file ONE issue covering that file (don't spam). If a violation is the same as one already-open issue, comment on the existing issue instead of opening a duplicate.

4) **Parallel pass (only if no actionable issues)**
- Run agents to find bugs/tech debt/docs drift.
- Deduplicate, open focused issues (repro + scope + acceptance criteria), restart triage.

5) **End-of-cycle output**
- `Done:` IDs + PRs
- `In Progress:` IDs + blockers
- `Closed as Outdated:` IDs + reason
- `New Issues:` IDs + priority
- `Next:` single next issue

## Guardrails

- No force-push to shared branches.
- No broad refactors without linked issue.
- Delete code files only if unreferenced and verified by search + build/tests.
- Delete outdated comments/docs when confirmed stale; keep surviving docs concise.
- Create new files only when absolutely necessary (prefer extending existing files).
- Zero-warning standard required (`-Wall -Wextra -Werror`, `/W4 /WX`).

## Definition of done

- Repro confirmed (or documented non-repro).
- Minimal fix merged or PR opened.
- If PR is ready and CI/CD is green: squash-merged to `main`, source branch deleted.
- Build/tests pass with zero warnings.
- Issue notes include root cause + verification.

## Monitoring (optional)

If you want your squad to watch external channels, enable monitor capabilities:

```bash
squad loop --monitor-email --monitor-teams
```

## Personality (optional)

If your squad has a specific voice or style, describe it here so each cycle
stays consistent.

Example: "Be concise. Use bullet points. Flag blockers clearly."

## Tips

- **Be specific.** Vague prompts produce vague results.
- **Set boundaries.** Tell the squad what NOT to do (e.g., "Don't send messages to anyone but me").
- **Start small.** Begin with one task per cycle, then expand.
- **Use frontmatter.** `interval` controls how often the loop runs. `timeout` caps each cycle.
