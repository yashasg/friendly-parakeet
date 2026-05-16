#pragma once

#include "../components/player.h"
#include "tags/tags.h"
#include <entt/entity/entity.hpp>
#include <cstdint>
#include <random>

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
// Sentinel values encode "no action needed":
//   target_shape  = Hexagon         → no shape change
//   target_lane   = -1              → no lane change
//   !wants_jump && !wants_slide     → no vertical action
//
// Per-sub-action completion is tracked as three independent boolean
// columns (formerly an `ActionDoneBit` bitmask). Per Fabian's existential-
// processing canon (.squad/decisions.md § DoD source-text grounding,
// Principle 4), control-flow discriminators — including bitmask
// discriminators and the former `VMode target_vertical` enum — become
// per-case tables. For a value type, that's per-case columns on the
// same row.
struct TestPlayerAction {
    entt::entity obstacle       = entt::null;
    float        timer          = 0.0f;
    float        arrival_time   = 0.0f;
    float        shape_not_before_time = 0.0f;

    Shape  target_shape         = Shape::Hexagon;
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
