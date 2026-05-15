#pragma once

#include "../components/player.h"
#include "tags/tags.h"
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
    float        shape_not_before_time = 0.0f;

    Shape  target_shape         = Shape::Hexagon;
    int8_t target_lane          = -1;
    VMode  target_vertical      = VMode::Grounded;

    ActionDoneBit done_flags = ActionDoneBit{};

};

// ── Test player state (context singleton) ────────────────────
// Hot state (accessed every frame): active, swipe_cooldown_timer,
//   action_count, actions[].
// Warm state (accessed during perception): skill, rng.
struct TestPlayerState {
    // ── Hot ──────────────────────────────────────────────────
    TestPlayerSkill skill   = TestPlayerSkill::Pro;
    bool            active  = false;

    // Delay between consecutive lane swipes (simulates human re-swipe time)
    static constexpr float SWIPE_COOLDOWN = 0.125f;  // 125ms (midpoint of 100-150ms)
    float swipe_cooldown_timer = 0.0f;

    static constexpr int MAX_ACTIONS = 32;
    TestPlayerAction actions[MAX_ACTIONS] = {};
    int              action_count = 0;

    // ── Warm ─────────────────────────────────────────────────
    std::mt19937     rng;
};
