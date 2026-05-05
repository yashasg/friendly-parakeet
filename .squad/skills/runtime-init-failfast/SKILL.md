---
name: "runtime-init-failfast"
description: "Enforce strict subsystem init contracts in game boot"
domain: "runtime"
confidence: "high"
source: "audit-derived"
---

## Pattern

### Fail fast on subsystem init

If a runtime subsystem init returns status, propagate that status to boot orchestration and stop startup immediately on failure. Never discard init status with `(void)` in adapter layers.

## Why

- Prevents silent degraded startup (game runs with partially initialized runtime).
- Keeps lifecycle policy explicit and testable (`init` success is a precondition for downstream setup).
- Avoids hidden drift where some systems assume readiness and others guard ad hoc.

## Apply It Here

- Change `platform::audio::init_audio_device()` to return `bool` and forward `InitAudioDevice()`.
- In `game_loop_init`, branch on audio init failure, request close, and return before `sfx_bank_init`/music setup.
- Add a regression test that simulates failed audio init and asserts early-exit shutdown path.
