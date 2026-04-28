# Decision: Squad log filenames must be Windows-safe (no colons)

**Author:** Kobayashi  
**Date:** 2026-05  
**Triggered by:** PR #43 Windows checkout failure

## Decision

All `.squad/` log and orchestration-log filenames that embed timestamps must use `-` instead of `:` as the time-separator (e.g. `2026-04-27T23-41-31Z-…` not `2026-04-27T23:41:31Z-…`). This applies to any tooling, agent, or template that auto-generates these files.

## Rationale

Windows NTFS rejects `:` in path components. Six tracked `.squad/` files with ISO-8601 timestamps caused `error: invalid path` on every Windows CI checkout, blocking the entire Windows matrix. The fix is trivial but must be enforced going forward so it doesn't recur.

## Scope

- `.squad/log/` filenames
- `.squad/orchestration-log/` filenames
- Any future squad-tooling that stamps files with timestamps

## Enforcement

Convention only (no linter configured). The Coordinator / spawning tooling should use the `-` format when naming files.
