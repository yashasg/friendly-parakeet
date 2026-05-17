// Tests for `approach_ring_envelope_system` per issue #1331. The legacy
// `ApproachRing::visible` bool was replaced with an `ApproachRingVisibleTag`
// (Fabian Principle 4 — per-frame existence as the visibility data). These
// tests verify both halves of the contract:
//   - When the nearest obstacle is inside the appear window the system
//     emplaces `ApproachRing` + `ApproachRingVisibleTag` on the matching
//     lane button.
//   - When it isn't (no obstacle, wrong shape, paused, etc.) the system
//     removes both — the renderer's `view<ApproachRing,
//     ApproachRingVisibleTag>` join keeps the lane dark.

#include "test_helpers.h"

#include <catch2/catch_test_macros.hpp>

#include "components/obstacle.h"
#include "components/song_state.h"
#include "components/ui.h"
#include "constants.h"
#include "systems/approach_ring_envelope_system.h"
#include "tags/tags.h"
#include "util/gameplay_hud_ring.h"

namespace {

entt::entity make_lane_button_circle(entt::registry& reg) {
    const auto e = reg.create();
    reg.emplace<LaneButtonTag>(e);
    reg.emplace<UiBounds>(e, UiBounds{140.0f, 100.0f});
    reg.emplace<UiShapeIconCircleTag>(e);
    return e;
}

entt::entity make_lane_button_square(entt::registry& reg) {
    const auto e = reg.create();
    reg.emplace<LaneButtonTag>(e);
    reg.emplace<UiBounds>(e, UiBounds{140.0f, 100.0f});
    reg.emplace<UiShapeIconSquareTag>(e);
    return e;
}

entt::entity spawn_obstacle_at_y_required_circle(entt::registry& reg, float y) {
    const auto e = reg.create();
    reg.emplace<ObstacleTag>(e);
    reg.emplace<Obstacle>(e);
    reg.emplace<RequiredShapeCircleTag>(e);
    reg.emplace<WorldPosition>(e, WorldPosition{{0.0f, y}});
    return e;
}

}  // namespace

TEST_CASE("approach_ring_envelope_system emplaces tag when obstacle is inside appear window") {
    auto reg = make_registry();
    auto circle_btn = make_lane_button_circle(reg);

    // SongState defaults give a positive ok_distance, so an obstacle parked
    // a small distance above the player line should be inside the appear
    // window.
    spawn_obstacle_at_y_required_circle(reg, constants::PLAYER_Y - 40.0f);

    approach_ring_envelope_system(reg);

    REQUIRE(reg.all_of<ApproachRingVisibleTag>(circle_btn));
    REQUIRE(reg.all_of<ApproachRing>(circle_btn));
}

TEST_CASE("approach_ring_envelope_system removes tag when no obstacle is present") {
    auto reg = make_registry();
    auto circle_btn = make_lane_button_circle(reg);
    // Pre-emplace stale visibility to prove the system clears it.
    reg.emplace<ApproachRing>(circle_btn, ApproachRing{});
    reg.emplace<ApproachRingVisibleTag>(circle_btn);

    approach_ring_envelope_system(reg);

    REQUIRE_FALSE(reg.all_of<ApproachRingVisibleTag>(circle_btn));
    REQUIRE_FALSE(reg.all_of<ApproachRing>(circle_btn));
}

TEST_CASE("approach_ring_envelope_system removes tag when obstacle is outside appear window") {
    auto reg = make_registry();
    auto circle_btn = make_lane_button_circle(reg);
    reg.emplace<ApproachRing>(circle_btn, ApproachRing{});
    reg.emplace<ApproachRingVisibleTag>(circle_btn);

    // Obstacle far above the player (greater than ok_distance) — outside
    // the appear window so `gameplay_hud_ring_cue` returns !visible.
    spawn_obstacle_at_y_required_circle(reg, constants::PLAYER_Y - 10000.0f);

    approach_ring_envelope_system(reg);

    REQUIRE_FALSE(reg.all_of<ApproachRingVisibleTag>(circle_btn));
    REQUIRE_FALSE(reg.all_of<ApproachRing>(circle_btn));
}

TEST_CASE("approach_ring_envelope_system isolates lanes by required shape tag") {
    auto reg = make_registry();
    auto circle_btn = make_lane_button_circle(reg);
    auto square_btn = make_lane_button_square(reg);

    // Only the circle lane has an obstacle.
    spawn_obstacle_at_y_required_circle(reg, constants::PLAYER_Y - 40.0f);

    approach_ring_envelope_system(reg);

    REQUIRE(reg.all_of<ApproachRingVisibleTag>(circle_btn));
    REQUIRE_FALSE(reg.all_of<ApproachRingVisibleTag>(square_btn));
}

TEST_CASE("approach_ring_envelope_system clears tag when not in playing phase") {
    auto reg = make_registry();
    auto circle_btn = make_lane_button_circle(reg);
    reg.emplace<ApproachRing>(circle_btn, ApproachRing{});
    reg.emplace<ApproachRingVisibleTag>(circle_btn);
    spawn_obstacle_at_y_required_circle(reg, constants::PLAYER_Y - 40.0f);

    set_test_phase<GamePhaseTitleTag>(reg);

    approach_ring_envelope_system(reg);

    REQUIRE_FALSE(reg.all_of<ApproachRingVisibleTag>(circle_btn));
    REQUIRE_FALSE(reg.all_of<ApproachRing>(circle_btn));
}
