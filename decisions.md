# Decisions Log

**Last updated:** 2026-05-08T18:08:07Z

## 2026-05-08: Shape Geometry Audit Post-Cleanup

### Decision
Keep shape_vertices.h CIRCLE table; schedule HEXAGON/SQUARE/TRIANGLE arrays for removal in future cleanup.

### Rationale
- CIRCLE table actively used by app/systems/game_render_system.cpp draw_floor_rings()
- Annulus triangles rendered via raylib 2D APIs inside 3D camera path cannot use direct raylib calls
- HEXAGON/SQUARE/TRIANGLE arrays only referenced in tests/benchmarks; removable without breaking game logic

### Owner
Architecture Review (Keaton, Keyser)

### Status
✅ Approved. No immediate action.

## 2026-05-08: Phase Transition Mechanism — Single Canonical Path (#482)

### Decision
UI screen controllers and input routing **must not** call `enter_phase()` directly.
They signal *intent* by setting `gs.transition_pending = true` and `gs.next_phase = X`.
The canonical state-machine system `game_state_system` performs the actual swap
(via `enter_phase`) on its own tick, alongside per-phase entry side effects.

`enter_phase()` remains an internal helper called only by:
- `game_state_system.cpp` — the canonical state-machine tick (deferred path).
- `session/play_session.cpp::setup_play_session` — explicit boot of a new session.
- `systems/game_state_terminal_phase_system.cpp` — terminal handoff.

The Paused→Playing resume case is dispatched through the same deferred path:
`game_state_system` recognises `phase == Paused && next_phase == Playing` and
swaps phases *without* re-running `setup_play_session` (which would discard
score/energy/obstacles).

### Rationale
- Eliminates the asymmetry called out by #482 where `paused_screen_controller`
  Resume used immediate `enter_phase` while Menu used the deferred path.
- Gives one well-defined place (`game_state_system`) to attach future per-phase
  entry side effects without hunting for stray `enter_phase` callers in UI code.
- Preserves ECS state-ownership: UI sets intent, the system owns the swap.

### Enforcement
`tests/test_phase_transition_canonical.cpp` scans all production sources under
`app/` (excluding build/generated/vendor-style paths) and fails the build if a
direct `enter_phase(` call appears outside the explicit allow-list above.

### Owner
squad:redfoot (UI controllers) + squad:keaton (state machine)

### Status
✅ Implemented in audit loop round 4.
