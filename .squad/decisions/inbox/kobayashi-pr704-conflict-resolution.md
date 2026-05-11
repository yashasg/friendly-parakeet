# Kobayashi Decision: PR #704 Conflict Resolution Process

## Context
PR #704 became dirty after `main` advanced (bundle PR #703 merged), while local checkout had unrelated in-progress edits and untracked runtime control files.

## Decision
For public squad branches, prefer **merge `origin/main` into the PR branch** over rebase to avoid rewriting published history. Before merge, stash only unrelated tracked local edits that could block/confuse conflict resolution, then reapply after merge commit is complete.

## Applied Steps
1. Verified branch/status and identified unrelated local changes.
2. Stashed unrelated tracked edits with pathspec (kept untracked runtime control files out of git).
3. Merged `origin/main` into `squad/restore-stashed-squad-state`.
4. Resolved all conflicts favoring already-landed `main` behavior where superseding, while preserving PR intent.
5. Ran full validation (`VCPKG_ROOT=/Users/yashasgujjar/vcpkg ./build.sh && ./build/shapeshifter_tests`).
6. Fixed one merge-introduced build break discovered by validation.
7. Committed merge with trailer and pushed branch.

## Why Reusable
This pattern preserves user WIP safely, avoids history rewrites on shared branches, and ensures conflict resolutions are verified by the repository’s canonical build/test command before push.
