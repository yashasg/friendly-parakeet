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
    auto& gs = reg.ctx().get<GameState>();
    gs.phase = GamePhase::Playing;
    gs.transition_pending = false;

    auto& song = reg.ctx().get<SongState>();
    song.playing = true;
    song.finished = true;

    auto& energy = reg.ctx().get<EnergyState>();
    energy.energy = 1.0f;

    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);

    game_state_system(reg, 1.0f / 60.0f);
    CHECK_FALSE(gs.transition_pending);

    reg.destroy(e);
    game_state_system(reg, 1.0f / 60.0f);
    CHECK(gs.transition_pending);
    CHECK(gs.next_phase == GamePhase::SongComplete);
}
