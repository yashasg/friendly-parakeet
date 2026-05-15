#pragma once

// System-private state for game_state_system. Stored in registry context, not
// emplaced on entities — this is per-system scratch, not component data.
// Relocated out of app/components/system_scratch.h (issue #1196).

// Tracks the last lane reported by the wasm smoke marker so the system can
// emit transitions without re-reporting on every frame. Only consumed under
// SHAPESHIFTER_WASM_SMOKE_MARKERS, but the type is initialized
// unconditionally so registry-context contracts stay consistent.
struct WasmSmokeLaneMarkerState {
    int last_lane = -1;
};
