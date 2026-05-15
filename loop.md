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

3) **Fabian drift check** (file `ecs_refactor` issues with file:line + principle #; do NOT fix this cycle)
- Canon: `.squad/decisions.md` § "DoD source-text grounding (Fabian)" Principles 0–4; cross-refs in #1203 / #1204.
- Checks: folder layout (only `components/ entities/ systems/ tags/ util/` under `app/`), existential processing (no `switch` on discriminator, no `virtual`/`override`/`dynamic_cast` — P0), components-as-data vs entities-as-factories, no inheritance (`: public ` outside raylib/EnTT wrappers), tags single-header (`app/tags/tags.h` only — P2), enum allowlist (`make check-enum-allowlist` from #1204 — P1), cyclomatic ratchet (`switch`/`if` branches in `app/` trend down only vs `.squad/decisions.md` § "Drift baselines").
- Cluster file-level hits into one issue. Comment on existing issue instead of duplicating.

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
- Zero-warning standard required (`-Wall -Wextra -Werror`). MSVC is not a supported toolchain.

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
