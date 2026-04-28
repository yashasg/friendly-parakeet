#pragma once

#include "../components/test_player.h"

inline bool test_player_shape_done(const TestPlayerAction& action) {
    return !!(action.done_flags & ActionDoneBit::Shape);
}

inline bool test_player_lane_done(const TestPlayerAction& action) {
    return !!(action.done_flags & ActionDoneBit::Lane);
}

inline bool test_player_vertical_done(const TestPlayerAction& action) {
    return !!(action.done_flags & ActionDoneBit::Vertical);
}

inline void test_player_mark_shape_done(TestPlayerAction& action) {
    action.done_flags |= ActionDoneBit::Shape;
}

inline void test_player_mark_lane_done(TestPlayerAction& action) {
    action.done_flags |= ActionDoneBit::Lane;
}

inline void test_player_mark_vertical_done(TestPlayerAction& action) {
    action.done_flags |= ActionDoneBit::Vertical;
}

inline bool test_player_needs_shape(const TestPlayerAction& action) {
    return action.target_shape != Shape::Hexagon && !test_player_shape_done(action);
}

inline bool test_player_needs_lane(const TestPlayerAction& action) {
    return action.target_lane >= 0 && !test_player_lane_done(action);
}

inline bool test_player_needs_vertical(const TestPlayerAction& action) {
    return action.target_vertical != VMode::Grounded && !test_player_vertical_done(action);
}

inline bool test_player_action_done(const TestPlayerAction& action) {
    const bool shape_done = (action.target_shape == Shape::Hexagon) ||
                            test_player_shape_done(action);
    const bool lane_done = (action.target_lane < 0) ||
                           test_player_lane_done(action);
    const bool vertical_done = (action.target_vertical == VMode::Grounded) ||
                               test_player_vertical_done(action);
    return shape_done && lane_done && vertical_done;
}

inline const SkillConfig& test_player_config(const TestPlayerState& state) {
    return SKILL_TABLE[static_cast<int>(state.skill)];
}

inline bool test_player_is_planned(const TestPlayerState& state, entt::entity e) {
    for (int i = 0; i < state.planned_count; ++i) {
        if (state.planned[i] == e) return true;
    }
    return false;
}

inline void test_player_mark_planned(TestPlayerState& state, entt::entity e) {
    if (state.planned_count < TestPlayerState::MAX_PLANNED) {
        state.planned[state.planned_count++] = e;
    }
}

inline void test_player_push_action(TestPlayerState& state, const TestPlayerAction& action) {
    if (state.action_count < TestPlayerState::MAX_ACTIONS) {
        state.actions[state.action_count++] = action;
    }
}

inline void test_player_remove_action(TestPlayerState& state, int idx) {
    if (idx >= 0 && idx < state.action_count) {
        state.actions[idx] = state.actions[state.action_count - 1];
        --state.action_count;
    }
}
