# Skill: Sync local main without touching dirty worktree

## Context

Use this when you must update local `main` to `origin/main` but cannot switch branches because the current worktree has local edits that would be overwritten.

## Pattern

1. Fetch first: `git fetch origin --prune`
2. Check fast-forward safety: `git merge-base --is-ancestor main origin/main`
3. If safe, update the branch ref directly: `git branch -f main origin/main`

This keeps current branch edits intact and still guarantees local `main` matches remote.

## Validation

```bash
git rev-parse --short main
git rev-parse --short origin/main
```

Both SHAs should match.
