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

## 2026-05-15: Round 7 Beatmap Quality Gates (#527 / #528 / #529 / #532)

### Decisions

1. **#527 (lead-in / trail-out caps).**  Authored obstacles must cover the
   playable timeline within `MAX_LEAD_IN_SEC` of `t=0` and within
   `MAX_TRAIL_OUT_SEC` of the song duration.  Caps are 8 s / 6 s / 4 s
   (lead) and 12 s / 10 s / 8 s (trail) for easy / medium / hard.  The
   wider trail cap absorbs songs whose final onsets fade out naturally.
   `_fill_silent_gaps` (level designer) and `validate_max_beat_gap.py`
   share these constants.

2. **#528 (no near-simultaneous distinct shape_gate pairs).**  The
   cross-layer 50 ms protection (#507) is preserved at the **selection**
   layer so directive 2026-05-10 invariants stay green; obstacle-level
   playability is enforced separately by
   `_collapse_simultaneous_obstacles` using
   `PLAYABILITY_MORPH_WINDOW_SEC = 0.120 s`.  When two emitted obstacles
   bind to different `(lane, shape)` within that window, the
   higher-flux event wins (ties broken by earlier `t`) and the
   discarded onset is recorded in `playability_collapsed_pairs`.

3. **#529 (difficulty ramp validator).**  The validator now performs
   two strict checks: (a) `count_ramp` requires
   `count(hard) ≥ count(medium) + 1 ≥ count(easy) + 2`; (b)
   `median_ioi_ramp` requires the **lower-quartile** IOI to be
   non-inverting tier-over-tier (≤ 50 ms inversion tolerance to absorb
   sparse-fill noise on bimodal songs).  The lower quartile measures
   dense-region pace, which is the difficulty signal players actually
   feel; the statistical median proved unstable on sparse songs whose
   IOI distribution is dominated by silent gaps.

4. **#532 (cluster ceiling).**  `MAX_SHAPE_CLUSTER_SIZE` = 4 (medium) /
   5 (hard).  The previous `not oversized_cluster_present` escape that
   self-disabled the cluster-chain run cap when a violation existed
   has been removed — both the cluster-chain cap (3) and the cluster
   size cap are strict, hard-fail gates.  No advisory escape exists at
   either gate.  The level-designer counterpart
   (`_enforce_max_shape_cluster_size` plus the post-rebalance
   `_thin_oversized_clusters_obstacles` and
   `_enforce_cluster_chain_cap_obstacles` passes) keeps the shipped
   beatmaps in compliance using onset-only mutation (re-shape rather
   than drop) so the IOI ramp in #529 is preserved.

### Owner
Level Designer (auto loop 7).

### Status
✅ Implemented; validators wired into CTest
(`validate_max_beat_gap`, `validate_difficulty_ramp`,
`validate_no_simultaneous_shape_gates`, `loop2_content_gates_strict`).
