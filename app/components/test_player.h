#pragma once

#include "player.h"
#include <entt/entity/entity.hpp>
#include <cstdint>
#include <random>

// ── Skill levels ─────────────────────────────────────────────
enum class TestPlayerSkill : uint8_t { Pro = 0, Good = 1, Bad = 2 };

struct SkillConfig {
    float vision_range;   // px from PLAYER_Y upward
    float reaction_min;   // seconds
    float reaction_max;   // seconds
    bool  aim_perfect;    // true = delay to hit Perfect window
};

inline constexpr SkillConfig SKILL_TABLE[] = {
    { 800.0f, 0.300f, 0.500f, true  },   // Pro
    { 600.0f, 0.500f, 0.800f, false },   // Good
    { 400.0f, 0.800f, 1.200f, false },   // Bad
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

    // Bitmask: bit 0 = shape_done, bit 1 = lane_done, bit 2 = vertical_done
    uint8_t done_flags          = 0;

    bool shape_done()    const { return (done_flags & 0x01) != 0; }
    bool lane_done()     const { return (done_flags & 0x02) != 0; }
    bool vertical_done() const { return (done_flags & 0x04) != 0; }

    void mark_shape_done()    { done_flags |= 0x01; }
    void mark_lane_done()     { done_flags |= 0x02; }
    void mark_vertical_done() { done_flags |= 0x04; }

    bool needs_shape()    const { return target_shape != Shape::Hexagon && !shape_done(); }
    bool needs_lane()     const { return target_lane >= 0 && !lane_done(); }
    bool needs_vertical() const { return target_vertical != VMode::Grounded && !vertical_done(); }

    bool all_done() const {
        bool s = (target_shape == Shape::Hexagon)       || shape_done();
        bool l = (target_lane < 0)                      || lane_done();
        bool v = (target_vertical == VMode::Grounded)   || vertical_done();
        return s && l && v;
    }
};

// ── Test player state (context singleton) ────────────────────
struct TestPlayerState {
    TestPlayerSkill skill   = TestPlayerSkill::Pro;
    bool            active  = false;
    uint32_t        frame_count = 0;

    // Delay between consecutive lane swipes (simulates human re-swipe time)
    static constexpr float SWIPE_COOLDOWN = 0.125f;  // 125ms (midpoint of 100-150ms)
    float swipe_cooldown_timer = 0.0f;

    static constexpr int MAX_ACTIONS = 16;
    TestPlayerAction actions[MAX_ACTIONS] = {};
    int              action_count = 0;

    static constexpr int MAX_PLANNED = 64;
    entt::entity     planned[MAX_PLANNED] = {};
    int              planned_count = 0;

    std::mt19937     rng;

    const SkillConfig& config() const { return SKILL_TABLE[static_cast<int>(skill)]; }

    bool is_planned(entt::entity e) const {
        for (int i = 0; i < planned_count; ++i) {
            if (planned[i] == e) return true;
        }
        return false;
    }

    void mark_planned(entt::entity e) {
        if (planned_count < MAX_PLANNED) {
            planned[planned_count++] = e;
        }
    }

    void push_action(const TestPlayerAction& a) {
        if (action_count < MAX_ACTIONS) {
            actions[action_count++] = a;
        }
    }

    void remove_action(int idx) {
        if (idx >= 0 && idx < action_count) {
            actions[idx] = actions[action_count - 1];
            --action_count;
        }
    }
};
