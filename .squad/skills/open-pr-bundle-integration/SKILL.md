# Skill: Open PR bundle integration branch

## Context
Use when many open PR heads must be integrated into one deterministic branch before review.

## Pattern
1. Create a dedicated worktree from `origin/main` and a dated integration branch.
2. Fetch each PR head to local refs (`pull/<n>/head:pr/<n>`), merge in ascending order, and log every status.
3. On conflict, resolve only when superseding intent is clear (duplicate PR chains, additive CI list updates, or pure formatting overlap). Document each conflict decision.
4. Run full repository validation with the project-standard commands and dependency env.
5. Push only after full integration + validation.

## Guardrails
- Never silently drop a PR; record merged/conflicted outcomes.
- Prefer additive CI regex resolution when multiple gate PRs overlap.
- Keep stricter runtime-safety behavior when RAII refactors overlap with resource-validity fixes.
