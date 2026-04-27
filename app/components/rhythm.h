#pragma once

// rhythm.h — Per-entity rhythm components, timing enums, and helpers.
// Loaded-once data (BeatMap) lives in beat_map.h.
// Runtime singletons (SongState, EnergyState, SongResults) live in song_state.h.
// This header re-exports both for backward compatibility.

#include "beat_map.h"
#include "song_state.h"
#include "player.h"
#include "obstacle.h"
#include <cstdint>
#include <magic_enum/magic_enum.hpp>

// ── Window Phase ────────────────────────────────────
enum class WindowPhase : uint8_t {
    Idle     = 0,
    MorphIn  = 1,
    Active   = 2,
    MorphOut = 3
};

// ── Timing Grade (emplaced on obstacle at collision) ─
enum class TimingTier : uint8_t {
    Bad,
    Ok,
    Good,
    Perfect,
};

// magic_enum::enum_name_v is a static_str with a null-terminated char array,
// so .data() is safe for printf-style %s formatting.
inline const char* ToString(TimingTier t) noexcept {
    const auto name = magic_enum::enum_name(t);
    return name.empty() ? "???" : name.data();
}

struct TimingGrade {
    TimingTier tier      = TimingTier::Bad;
    float      precision = 0.0f;  // 0.0 = edge, 1.0 = dead center
};

// ── Beat Info (per-obstacle, carries chart data) ─────
struct BeatInfo {
    int   beat_index   = 0;
    float arrival_time = 0.0f;
    float spawn_time   = 0.0f;
};

// ── Helper: window scale factor from tier ────────────
// Better timing → smaller scale → remaining Active window collapses sooner.
// collision_system applies this via window_start adjustment (scale < 1.0 path).
// Spec: rhythm-spec.md §5/§7. BAD is treated as a miss; window is left unchanged.
inline float window_scale_for_tier(TimingTier tier) {
    switch (tier) {
        case TimingTier::Perfect: return 0.50f;
        case TimingTier::Good:    return 0.75f;
        case TimingTier::Ok:      return 1.00f;
        case TimingTier::Bad:     return 1.00f;
    }
    return 1.00f;
}

// ── Helper: timing multiplier from tier ─────────────
inline float timing_multiplier(TimingTier tier) {
    switch (tier) {
        case TimingTier::Perfect: return 1.50f;
        case TimingTier::Good:    return 1.00f;
        case TimingTier::Ok:      return 0.50f;
        case TimingTier::Bad:     return 0.25f;
    }
    return 0.25f;
}

// ── Helper: compute timing tier from pct_from_peak ──
inline TimingTier compute_timing_tier(float pct_from_peak) {
    if (pct_from_peak <= 0.25f) return TimingTier::Perfect;
    if (pct_from_peak <= 0.50f) return TimingTier::Good;
    if (pct_from_peak <= 0.75f) return TimingTier::Ok;
    return TimingTier::Bad;
}
