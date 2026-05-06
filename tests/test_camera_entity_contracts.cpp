// Camera entity contract tests — camera cleanup gate.
//
// The camera cleanup (Keyser) replaces the old reg.ctx().emplace<GameCamera>
// singleton with an entity created via spawn_game_camera(reg).

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>
#include <stdexcept>
#include <type_traits>

#include "entities/camera_entity.h"

static_assert(std::is_standard_layout_v<GameCamera>,
              "camera-cleanup gate: GameCamera must be standard-layout for EnTT pool.");

static_assert(std::is_default_constructible_v<GameCamera>,
              "camera-cleanup gate: GameCamera must be default-constructible.");

TEST_CASE("camera entity: single GameCamera entity queryable via view",
          "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;
    spawn_game_camera(reg);

    auto view = reg.view<GameCamera>();
    int count = 0;
    for (auto [entity, gc] : view.each()) {
        (void)entity;
        (void)gc;
        ++count;
    }
    CHECK(count == 1);
}

TEST_CASE("camera entity: spawn rejects duplicate GameCamera entities",
          "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;
    spawn_game_camera(reg);

    CHECK_THROWS_AS(spawn_game_camera(reg), std::logic_error);
}

TEST_CASE("camera entity: accessor rejects manually duplicated camera entities",
          "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;
    spawn_game_camera(reg);
    reg.emplace<GameCamera>(reg.create());

    CHECK_THROWS_AS(game_camera(reg), std::logic_error);
}

TEST_CASE("camera entity: accessor game_camera() returns mutable reference",
          "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;
    spawn_game_camera(reg);

    GameCamera& gc = game_camera(reg);
    (void)gc;
    SUCCEED("game_camera() accessor did not crash");
}

TEST_CASE("camera entity: accessor throws before camera entity is spawned",
          "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;

    CHECK_THROWS_AS(game_camera(reg), std::logic_error);
}

TEST_CASE("camera entity: destroying game camera removes accessor target",
          "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;
    spawn_game_camera(reg);

    const auto game_ent = reg.view<GameCamera>().front();
    reg.destroy(game_ent);

    CHECK_FALSE(reg.valid(game_ent));
    CHECK_THROWS_AS(game_camera(reg), std::logic_error);
}
