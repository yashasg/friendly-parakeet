#pragma once

#include "../components/rhythm.h"
#include "../constants.h"
#include "../tags/tags.h"

#include <cmath>
#include <entt/entt.hpp>
#include <raylib.h>

constexpr float kTimingBpmCeiling = 180.0f;
constexpr float kTimingPerfectSeconds = 0.050f;
constexpr float kTimingGoodSeconds = 0.100f;
constexpr float kTimingOkSeconds = 0.150f;
constexpr float kTimingGradeEpsilonSeconds = 0.0005f;
constexpr float kDefaultTimingBpm = 120.0f;
constexpr int kDefaultTimingLeadBeats = 4;

// Per-tier multiplier and window-scale lookups, keyed by absolute delta-seconds
// instead of a discriminator (issue #1202/#1204). The threshold ladder is the
// per-value table — Perfect/Good/Ok/Bad are zero-column tags emplaced alongside
// these returns; the multiplier/scale columns are inline here.
//
// Better timing -> smaller window scale -> remaining Active window collapses
// sooner. collision_system applies this via window_start adjustment when the
// returned scale is < 1.0. Spec: rhythm-spec.md §5/§7. BAD is a scored hit
// with no window-collapse reward.
inline float window_scale_from_delta_abs(float delta_seconds_abs) {
    if (delta_seconds_abs <= kTimingPerfectSeconds + kTimingGradeEpsilonSeconds) return 0.50f;
    if (delta_seconds_abs <= kTimingGoodSeconds    + kTimingGradeEpsilonSeconds) return 0.75f;
    return 1.00f;  // Ok and Bad: neutral
}

inline float timing_multiplier_from_delta_abs(float delta_seconds_abs) {
    if (delta_seconds_abs <= kTimingPerfectSeconds + kTimingGradeEpsilonSeconds) return 1.50f;
    if (delta_seconds_abs <= kTimingGoodSeconds    + kTimingGradeEpsilonSeconds) return 1.00f;
    if (delta_seconds_abs <= kTimingOkSeconds      + kTimingGradeEpsilonSeconds) return 0.50f;
    return 0.25f;
}

// Emplace the matching tier tag for the given absolute delta-seconds. Removes
// any other tier tag already on the entity so an entity carries exactly one
// tier tag at a time (existential processing — issue #1202/#1204).
inline void emplace_timing_tier_tag_for_delta_abs(entt::registry& reg,
                                                  entt::entity entity,
                                                  float delta_seconds_abs) {
    reg.remove<TimingPerfectTag>(entity);
    reg.remove<TimingGoodTag>(entity);
    reg.remove<TimingOkTag>(entity);
    reg.remove<TimingBadTag>(entity);
    if (delta_seconds_abs <= kTimingPerfectSeconds + kTimingGradeEpsilonSeconds) {
        reg.emplace<TimingPerfectTag>(entity);
    } else if (delta_seconds_abs <= kTimingGoodSeconds + kTimingGradeEpsilonSeconds) {
        reg.emplace<TimingGoodTag>(entity);
    } else if (delta_seconds_abs <= kTimingOkSeconds + kTimingGradeEpsilonSeconds) {
        reg.emplace<TimingOkTag>(entity);
    } else {
        reg.emplace<TimingBadTag>(entity);
    }
}

// True iff `entity` carries any of the four tier tags.
inline bool has_any_timing_tier_tag(const entt::registry& reg, entt::entity entity) {
    return reg.any_of<TimingPerfectTag, TimingGoodTag, TimingOkTag, TimingBadTag>(entity);
}

// Per-tier emplace helpers — equivalent to manually emplacing TimingGrade
// plus the matching tier tag. Used by tests and by spots where the tier is
// known statically.
inline void emplace_timing_perfect(entt::registry& reg, entt::entity e, float precision) {
    reg.emplace_or_replace<TimingGrade>(e, TimingGrade{precision});
    reg.remove<TimingGoodTag>(e);
    reg.remove<TimingOkTag>(e);
    reg.remove<TimingBadTag>(e);
    reg.emplace_or_replace<TimingPerfectTag>(e);
}
inline void emplace_timing_good(entt::registry& reg, entt::entity e, float precision) {
    reg.emplace_or_replace<TimingGrade>(e, TimingGrade{precision});
    reg.remove<TimingPerfectTag>(e);
    reg.remove<TimingOkTag>(e);
    reg.remove<TimingBadTag>(e);
    reg.emplace_or_replace<TimingGoodTag>(e);
}
inline void emplace_timing_ok(entt::registry& reg, entt::entity e, float precision) {
    reg.emplace_or_replace<TimingGrade>(e, TimingGrade{precision});
    reg.remove<TimingPerfectTag>(e);
    reg.remove<TimingGoodTag>(e);
    reg.remove<TimingBadTag>(e);
    reg.emplace_or_replace<TimingOkTag>(e);
}
inline void emplace_timing_bad(entt::registry& reg, entt::entity e, float precision) {
    reg.emplace_or_replace<TimingGrade>(e, TimingGrade{precision});
    reg.remove<TimingPerfectTag>(e);
    reg.remove<TimingGoodTag>(e);
    reg.remove<TimingOkTag>(e);
    reg.emplace_or_replace<TimingBadTag>(e);
}

// Remove TimingGrade and all four tier tags. Used by scoring/cleanup paths
// that previously did `reg.remove<TimingGrade>(e)` to clear the tier row.
inline void remove_timing_grade_and_tags(entt::registry& reg, entt::entity e) {
    reg.remove<TimingGrade>(e);
    reg.remove<TimingPerfectTag>(e);
    reg.remove<TimingGoodTag>(e);
    reg.remove<TimingOkTag>(e);
    reg.remove<TimingBadTag>(e);
}

inline void song_state_compute_derived(SongState& s) {
    if (!std::isfinite(s.bpm) || s.bpm <= 0.0f) {
        TraceLog(LOG_WARNING, "Invalid SongState bpm %.3f; using %.1f", s.bpm, kDefaultTimingBpm);
        s.bpm = kDefaultTimingBpm;
    }
    if (s.lead_beats <= 0) {
        TraceLog(LOG_WARNING, "Invalid SongState lead_beats %d; using %d", s.lead_beats, kDefaultTimingLeadBeats);
        s.lead_beats = kDefaultTimingLeadBeats;
    }

    s.beat_period     = 60.0f / s.bpm;
    s.lead_time       = static_cast<float>(s.lead_beats) * s.beat_period;
    if (!std::isfinite(s.lead_time) || s.lead_time <= 0.0f) {
        TraceLog(LOG_WARNING, "Invalid derived lead_time %.3f; using default timing", s.lead_time);
        s.bpm = kDefaultTimingBpm;
        s.lead_beats = kDefaultTimingLeadBeats;
        s.beat_period = 60.0f / s.bpm;
        s.lead_time = static_cast<float>(s.lead_beats) * s.beat_period;
    }

    s.scroll_speed    = constants::APPROACH_DIST / s.lead_time;

    constexpr float BASE_MORPH_BEATS  = 0.2f;
    constexpr float MIN_MORPH         = 0.06f;
    constexpr float WINDOW_HALF_CEILING = 0.5f * (60.0f / kTimingBpmCeiling);

    // Fixed judgment window policy (press-time grading):
    // keep active window at +/-150ms while respecting the selected BPM ceiling.
    float half_window = kTimingOkSeconds;
    if (half_window > WINDOW_HALF_CEILING) {
        half_window = WINDOW_HALF_CEILING;
    }
    s.window_duration = half_window * 2.0f;
    s.half_window     = s.window_duration / 2.0f;

    s.morph_duration  = BASE_MORPH_BEATS * s.beat_period;
    if (s.morph_duration < MIN_MORPH) s.morph_duration = MIN_MORPH;
}
