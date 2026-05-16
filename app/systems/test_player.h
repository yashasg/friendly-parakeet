#pragma once

#include "tags/tags.h"
#include <entt/entity/entity.hpp>
#include <cstdint>
#include <random>

// Per-shape press dispatch row (defined in test_player_system.cpp).
// Forward-declared so this header does not depend on entt::dispatcher or
// the input-event types. Identity-is-the-choice (Fabian): the pointer's
// value IS the shape choice — there is no enum compared at any consumer
// site. See `app/systems/test_player_system.cpp` for the table.
struct ShapePressSpec;

// ── Skill config (per-skill data row) ────────────────────────
// Per Fabian's existential-processing canon (.squad/decisions.md
// § DoD source-text grounding, Principle 4) and #1204's table-aware
// mechanic, the former `TestPlayerSkill` enum was a pure lookup-key
// index into a 3-row `SKILL_TABLE[]`. Both the enum and the index
// dispatch are deleted; each former enum value is now a named
// `SkillConfig` constant that callers pick directly.
struct SkillConfig {
    float       vision_range;   // px from PLAYER_Y upward
    float       reaction_min;   // seconds
    float       reaction_max;   // seconds
    bool        aim_perfect;    // true = delay shape presses to hit Perfect window
    const char* name;           // CLI key + session-log key
};

inline constexpr SkillConfig SKILL_PRO  { 800.0f, 0.300f, 0.500f, true,  "pro"  };
inline constexpr SkillConfig SKILL_GOOD { 600.0f, 0.500f, 0.800f, false, "good" };
inline constexpr SkillConfig SKILL_BAD  { 400.0f, 0.800f, 1.200f, false, "bad"  };

// ── Queued action (value type, NOT a component) ──────────────
// Sentinel-free choice encoding:
//   shape_press   = nullptr         → no shape change (identity-is-the-choice)
//   target_lane   = -1              → no lane change
//   !wants_jump && !wants_slide     → no vertical action
//
// Per-sub-action completion is tracked as three independent boolean
// columns (formerly an `ActionDoneBit` bitmask). Per Fabian's existential-
// processing canon (.squad/decisions.md § DoD source-text grounding,
// Principle 4), control-flow discriminators — including the former
// `Shape target_shape = Shape::Hexagon` sentinel (issue #1202 PR C3),
// the `ActionDoneBit` bitmask, and the `VMode target_vertical` enum —
// become per-case tables. For a value type, the per-shape table is
// reached through `shape_press` — a pointer into the per-row dispatch
// table in `test_player_system.cpp`. `nullptr` means "no shape required"
// (replaces the former `Shape::Hexagon` sentinel compare); a non-null
// pointer carries both the press-enqueue function and the log labels
// for that shape, so consumers never branch on a Shape value.
struct TestPlayerAction {
    entt::entity obstacle       = entt::null;
    float        timer          = 0.0f;
    float        arrival_time   = 0.0f;
    float        shape_not_before_time = 0.0f;

    const ShapePressSpec* shape_press = nullptr;
    int8_t target_lane          = -1;
    bool   wants_jump           = false;
    bool   wants_slide          = false;

    bool shape_done    = false;
    bool lane_done     = false;
    bool vertical_done = false;
};

// ── Test player state (context singleton) ────────────────────
// Hot state (accessed every frame): active, swipe_cooldown_timer,
//   action_count, actions[].
// Warm state (accessed during perception): skill, rng.
struct TestPlayerState {
    // ── Hot ──────────────────────────────────────────────────
    SkillConfig skill   = SKILL_PRO;
    bool        active  = false;

    // Delay between consecutive lane swipes (simulates human re-swipe time)
    static constexpr float SWIPE_COOLDOWN = 0.125f;  // 125ms (midpoint of 100-150ms)
    float swipe_cooldown_timer = 0.0f;

    static constexpr int MAX_ACTIONS = 32;
    TestPlayerAction actions[MAX_ACTIONS] = {};
    int              action_count = 0;

    // ── Warm ─────────────────────────────────────────────────
    std::mt19937     rng;
};
