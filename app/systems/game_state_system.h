#pragma once

// System-private state for game_state_system. Stored in registry context, not
// emplaced on entities — this is per-system scratch, not component data.
// Relocated out of app/components/system_scratch.h (issue #1196).

// Tracks the last lane reported by the wasm smoke marker so the system can
// emit transitions without re-reporting on every frame. Only consumed under
// SHAPESHIFTER_WASM_SMOKE_MARKERS.
//
// Per Fabian Principle 3 (.squad/decisions.md § 9 — NULL columns), the
// "no lane reported yet" state is encoded as row ABSENCE in `reg.ctx()`, not
// a sentinel `last_lane = -1`. Presence of the row IS the precondition for
// reading `lane`; the row is erased on phase exit / missing player to reset.
struct WasmSmokeLastLane {
    int lane;
};
