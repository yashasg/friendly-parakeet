// Camera entity contract tests — camera cleanup gate.
//
// The camera cleanup (Keyser) replaces two reg.ctx() singletons:
//   reg.ctx().emplace<GameCamera>  →  entity via spawn_game_camera(reg)
//   reg.ctx().emplace<UICamera>    →  entity via spawn_ui_camera(reg)
//
// These tests define and verify the expected contract for the post-cleanup
// entity model against the already-landed camera_entity.h factories.
//
// components/camera.h is a compatibility forwarding header only. New code
// should include entities/camera_entity.h or rendering/camera_resources.h.

#include <catch2/catch_test_macros.hpp>
#include <type_traits>
#include <entt/entt.hpp>
#include <stdexcept>

#include "entities/camera_entity.h"  // GameCamera, UICamera, spawn_*

// ── Type-trait gates ─────────────────────────────────────────────────────────
// These fire at compile time; no camera.h include required after cleanup.

static_assert(std::is_standard_layout_v<GameCamera>,
    "camera-cleanup gate: GameCamera must be standard-layout for EnTT pool.");

static_assert(std::is_standard_layout_v<UICamera>,
    "camera-cleanup gate: UICamera must be standard-layout for EnTT pool.");

static_assert(std::is_default_constructible_v<GameCamera>,
    "camera-cleanup gate: GameCamera must be default-constructible.");

static_assert(std::is_default_constructible_v<UICamera>,
    "camera-cleanup gate: UICamera must be default-constructible.");

static_assert(!std::is_same_v<GameCamera, UICamera>,
    "camera-cleanup gate: GameCamera and UICamera must be distinct types.");

// ── Entity factory contract ──────────────────────────────────────────────────

TEST_CASE("camera entity: single GameCamera entity queryable via view",
          "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;
    spawn_game_camera(reg);

    auto view = reg.view<GameCamera>();
    int count = 0;
    for (auto [entity, gc] : view.each()) { (void)gc; ++count; }
    CHECK(count == 1);
}

TEST_CASE("camera entity: single UICamera entity queryable via view",
          "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;
    spawn_ui_camera(reg);

    auto view = reg.view<UICamera>();
    int count = 0;
    for (auto [entity, uc] : view.each()) { (void)uc; ++count; }
    CHECK(count == 1);
}

TEST_CASE("camera entity: game_camera and ui_camera entities are distinct",
           "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;
    spawn_game_camera(reg);
    spawn_ui_camera(reg);

    auto gv = reg.view<GameCamera>();
    auto uv = reg.view<UICamera>();

    REQUIRE(!gv.empty());
    REQUIRE(!uv.empty());

    auto game_ent = gv.front();
    auto ui_ent   = uv.front();
    CHECK(game_ent != ui_ent);

    // Each entity carries exactly one camera type.
    CHECK(reg.all_of<GameCamera>(game_ent));
    CHECK_FALSE(reg.all_of<UICamera>(game_ent));

    CHECK(reg.all_of<UICamera>(ui_ent));
    CHECK_FALSE(reg.all_of<GameCamera>(ui_ent));
}

TEST_CASE("camera entity: spawn functions reject duplicate camera entities",
          "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;
    spawn_game_camera(reg);
    spawn_ui_camera(reg);

    CHECK_THROWS_AS(spawn_game_camera(reg), std::logic_error);
    CHECK_THROWS_AS(spawn_ui_camera(reg), std::logic_error);
}

TEST_CASE("camera entity: accessors reject manually duplicated camera entities",
          "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;
    spawn_game_camera(reg);
    spawn_ui_camera(reg);
    reg.emplace<GameCamera>(reg.create());
    reg.emplace<UICamera>(reg.create());

    CHECK_THROWS_AS(game_camera(reg), std::logic_error);
    CHECK_THROWS_AS(ui_camera(reg), std::logic_error);
}

TEST_CASE("camera entity: no entity holds both GameCamera and UICamera",
           "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;
    spawn_game_camera(reg);
    spawn_ui_camera(reg);

    auto dual_view = reg.view<GameCamera, UICamera>();
    int dual_count = 0;
    for (auto [e, gc, uc] : dual_view.each()) { (void)gc; (void)uc; ++dual_count; }
    CHECK(dual_count == 0);
}

TEST_CASE("camera entity: accessor game_camera() returns mutable reference",
           "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;
    spawn_game_camera(reg);

    // Must not crash; returned reference must be valid.
    GameCamera& gc = game_camera(reg);
    (void)gc;
    SUCCEED("game_camera() accessor did not crash");
}

TEST_CASE("camera entity: accessors throw before camera entities are spawned",
          "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;

    CHECK_THROWS_AS(game_camera(reg), std::logic_error);
    CHECK_THROWS_AS(ui_camera(reg), std::logic_error);
}

TEST_CASE("camera entity: accessor ui_camera() returns mutable reference",
           "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;
    spawn_ui_camera(reg);

    UICamera& uc = ui_camera(reg);
    (void)uc;
    SUCCEED("ui_camera() accessor did not crash");
}

TEST_CASE("camera entity: destroying game_cam entity does not affect ui_cam",
          "[camera][entity_contract][camera_cleanup]") {
    entt::registry reg;
    spawn_game_camera(reg);
    spawn_ui_camera(reg);

    auto game_ent = reg.view<GameCamera>().front();
    auto ui_ent   = reg.view<UICamera>().front();

    reg.destroy(game_ent);

    CHECK_FALSE(reg.valid(game_ent));
    CHECK(reg.valid(ui_ent));
    CHECK(reg.all_of<UICamera>(ui_ent));
}
