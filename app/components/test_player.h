#pragma once

#include "player.h"
#include <entt/entity/entity.hpp>
#include <entt/core/enum.hpp>
#include <cstdint>
#include <random>

// ── Skill levels ─────────────────────────────────────────────
enum class TestPlayerSkill : uint8_t { Pro = 0, Good = 1, Bad = 2 };

struct SkillConfig {
    float vision_range;   // px from PLAYER_Y upward
    float reaction_min;   // seconds
    float reaction_max;   // seconds
    bool  aim_perfect;    // true = delay shape presses to hit Perfect window
};

inline constexpr SkillConfig SKILL_TABLE[] = {
    { 800.0f, 0.300f, 0.500f, true  },   // Pro
    { 600.0f, 0.500f, 0.800f, false },   // Good
    { 400.0f, 0.800f, 1.200f, false },   // Bad
};

// ── Done-flag bits for TestPlayerAction ──────────────────────
// Power-of-two values + _entt_enum_as_bitmask activates EnTT's
// typed |/&/^ operators, replacing raw uint8_t literal helpers.
enum class ActionDoneBit : uint8_t {
    Shape    = 1 << 0,
    Lane     = 1 << 1,
    Vertical = 1 << 2,
    _entt_enum_as_bitmask
};

// ── Queued action (value type, NOT a component) ──────────────
// Sentinel values encode "no action needed":
//   target_shape  = Hexagon   → no shape change
//   target_lane   = -1        → no lane change
//   target_vertical = Grounded → no jump/slide
struct TestPlayerAction {
    entt::entity obstacle       = entt::null;
    float        timer          = 0.0f;
    float        arrival_time   = 0.0f;

    Shape  target_shape         = Shape::Hexagon;
    int8_t target_lane          = -1;
    VMode  target_vertical      = VMode::Grounded;

    ActionDoneBit done_flags = ActionDoneBit{};

};

// ── Test player state (context singleton) ────────────────────
// Hot state (accessed every frame): active, frame_count, swipe_cooldown_timer,
//   action_count, actions[].
// Warm state (accessed during perception): skill, rng.
// Cold state (accessed during scan + stale-entity cleanup): planned[], planned_count.
struct TestPlayerState {
    // ── Hot ──────────────────────────────────────────────────
    TestPlayerSkill skill   = TestPlayerSkill::Pro;
    bool            active  = false;
    uint32_t        frame_count = 0;

    // Delay between consecutive lane swipes (simulates human re-swipe time)
    static constexpr float SWIPE_COOLDOWN = 0.125f;  // 125ms (midpoint of 100-150ms)
    float swipe_cooldown_timer = 0.0f;

    static constexpr int MAX_ACTIONS = 32;
    TestPlayerAction actions[MAX_ACTIONS] = {};
    int              action_count = 0;

    // ── Warm ─────────────────────────────────────────────────
    std::mt19937     rng;

    // ── Cold ─────────────────────────────────────────────────
    // LIFETIME CONTRACT: planned[] stores raw entt::entity handles for obstacles
    // that have already been queued for action processing. These handles can
    // become stale between ticks if an obstacle is destroyed (e.g., scored,
    // culled, or registry cleared on session restart).
    //
    // Invariant upheld by the system:
    //   - test_player_clean_planned() is called at the START of every system
    //     tick and removes any entry for which reg.valid(e) is false.
    //   - After cleanup, all entries in planned[0..planned_count) are valid
    //     handles for that tick only. Do not cache the validity result across
    //     tick boundaries.
    //   - Actions that reference an obstacle via TestPlayerAction::obstacle
    //     also guard with reg.valid() before any component access.
    static constexpr int MAX_PLANNED = 256;
    entt::entity     planned[MAX_PLANNED] = {};
    int              planned_count = 0;

};
