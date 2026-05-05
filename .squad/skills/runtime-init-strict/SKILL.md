---
name: "runtime-init-strict"
description: "Enforce explicit init success contracts and partial-init-safe shutdown."
domain: "runtime, lifecycle"
confidence: "high"
source: "audit-derived"
---

## Pattern

### Strict runtime init/shutdown contract

For app lifecycle entrypoints, make initialization report success explicitly and gate run/shutdown on that result. Shutdown must tolerate partial init and skip teardown paths whose resources/context were never created.

## Why

- Prevents calling teardown on missing registry context and crashing on early init failures.
- Makes fatal bring-up failures visible to callers and CI smoke paths.
- Avoids "request close + continue anyway" control flow that hides initialization faults.

## Apply It Here

- Change `game_loop_init` to return `bool` (or equivalent status).
- In `main.cpp`, run and shutdown only when init succeeds; return non-zero on init failure.
- In `game_loop_shutdown`, guard context/resource access behind `reg.ctx().find<...>()` when init may have returned early.
