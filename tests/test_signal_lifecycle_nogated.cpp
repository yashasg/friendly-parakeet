#include <catch2/catch_test_macros.hpp>
#include "test_helpers.h"

TEST_CASE("obstacle_runtime: obstacle view emptiness tracks obstacle entity lifetime",
          "[obstacle][runtime]") {
    auto reg = make_registry();
    REQUIRE(reg.view<ObstacleTag>().empty());

    auto e1 = reg.create();
    reg.emplace<ObstacleTag>(e1);
    CHECK_FALSE(reg.view<ObstacleTag>().empty());

    reg.destroy(e1);
    CHECK(reg.view<ObstacleTag>().empty());
}

TEST_CASE("game_state: finished song waits for obstacle drain before SongComplete",
          "[game_state][runtime]") {
    auto reg = make_registry();
    set_test_phase<GamePhasePlayingTag>(reg);
    clear_next_phase_tags(reg);

    auto& song = reg.ctx().get<SongState>();
    reg.ctx().emplace<SongPlayingTag>();
    reg.ctx().emplace<SongFinishedTag>();
    (void)song;

    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 1.0f;

    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);

    game_state_system(reg, 1.0f / 60.0f);
    CHECK_FALSE(is_phase_transition_pending(reg));

    reg.destroy(e);
    game_state_system(reg, 1.0f / 60.0f);
    CHECK(reg.ctx().contains<NextPhaseSongCompleteTag>());
}
