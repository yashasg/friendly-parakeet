// Tests for the Redfoot TestFlight UI pass — issues #168/#196 (game over
// reason) and #198 (settings toggles show state on the button face).
//
// These tests now focus on runtime-live coverage only:
//   * Schema/content checks that remain relevant to current UI flows.
//   * Wiring checks that collision and energy systems set GameOverState.cause.

#include <catch2/catch_test_macros.hpp>
#include <entt/entity/registry.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#include "components/game_state.h"
#include "components/obstacle.h"
#include "components/player.h"
#include "components/rendering.h"
#include "components/rhythm.h"
#include "components/scoring.h"
#include "util/settings.h"
#include "components/song_state.h"
#include "components/transform.h"
#include "constants.h"
#include "systems/all_systems.h"

using json = nlohmann::json;

namespace {
const json* find_by_id(const json& screen, const std::string& id) {
    if (!screen.contains("elements")) return nullptr;
    for (const auto& el : screen["elements"]) {
        if (el.value("id", "") == id) return &el;
    }
    return nullptr;
}

json load_screen(const std::string& path) {
    std::ifstream f(path);
    REQUIRE(f.is_open());
    return json::parse(f);
}
}  // namespace

// ── Content: game_over.json layout ──────────────────────────────────────────

TEST_CASE("redfoot/#168: existing game_over buttons keep their original positions",
          "[ui][redfoot][game_over]") {
    auto screen = load_screen("content/ui/screens/game_over.json");

    const json* restart = find_by_id(screen, "restart_button");
    const json* level   = find_by_id(screen, "level_select_button");
    const json* menu    = find_by_id(screen, "menu_button");
    REQUIRE(restart != nullptr);
    REQUIRE(level   != nullptr);
    REQUIRE(menu    != nullptr);

    // Original y_n values from the shipped layout — muscle memory must
    // not move.
    CHECK(restart->at("y_n").get<float>() == 0.6797f);
    CHECK(level  ->at("y_n").get<float>() == 0.7305f);
    CHECK(menu   ->at("y_n").get<float>() == 0.7812f);

    // Action wiring preserved.
    CHECK(restart->value("action", "") == "restart");
    CHECK(level  ->value("action", "") == "level_select");
    CHECK(menu   ->value("action", "") == "main_menu");
}

// ── Wiring: collision sets DeathCause for misses and bar hits ───────────────

namespace {
// Spawn a player aligned with an obstacle so the collision system's
// distance gate is satisfied; force a miss by leaving the player as
// Hexagon (matches no required shape, fails LowBar/HighBar action checks).
void spawn_aligned_player(entt::registry& reg, float y) {
    auto p = reg.create();
    reg.emplace<PlayerTag>(p);
    reg.emplace<WorldTransform>(p, WorldTransform{{constants::LANE_X[1], y}});
    PlayerShape ps; ps.current = Shape::Hexagon; ps.previous = Shape::Hexagon;
    reg.emplace<PlayerShape>(p, ps);
    ShapeWindow sw{};
    sw.phase = WindowPhase::Idle;
    reg.emplace<ShapeWindow>(p, sw);
    Lane lane{}; lane.current = 1; lane.target = -1;
    reg.emplace<Lane>(p, lane);
    reg.emplace<VerticalState>(p);
}

entt::entity spawn_obstacle(entt::registry& reg, ObstacleKind kind, float y) {
    auto e = reg.create();
    reg.emplace<ObstacleTag>(e);
    if (kind == ObstacleKind::LowBar || kind == ObstacleKind::HighBar) {
        reg.emplace<ObstacleScrollZ>(e, y);
        reg.emplace<WorldTransform>(e, WorldTransform{{constants::SCREEN_W_F * 0.5f, y}});
    } else {
        reg.emplace<Position>(e, constants::LANE_X[1], y);
        reg.emplace<WorldTransform>(e, WorldTransform{{constants::LANE_X[1], y}});
    }
    reg.emplace<Obstacle>(e, kind, int16_t{0});
    return e;
}
}  // namespace

TEST_CASE("redfoot/#168: collision flags HitABar when a bar passes ungraded",
          "[ui][redfoot][game_over][wiring]") {
    entt::registry reg;
    reg.ctx().emplace<GameState>().phase = GamePhase::Playing;
    reg.ctx().emplace<EnergyState>();
    reg.ctx().emplace<SongResults>();
    reg.ctx().emplace<GameOverState>();

    spawn_aligned_player(reg, constants::PLAYER_Y);
    auto bar = spawn_obstacle(reg, ObstacleKind::LowBar, constants::PLAYER_Y);
    reg.emplace<RequiredVAction>(bar, VMode::Jumping);

    collision_system(reg, 0.016f);

    auto& gos = reg.ctx().get<GameOverState>();
    CHECK(gos.cause == DeathCause::HitABar);
}

TEST_CASE("redfoot/#168: collision flags MissedABeat for a missed shape gate",
          "[ui][redfoot][game_over][wiring]") {
    entt::registry reg;
    reg.ctx().emplace<GameState>().phase = GamePhase::Playing;
    reg.ctx().emplace<EnergyState>();
    reg.ctx().emplace<SongResults>();
    reg.ctx().emplace<GameOverState>();

    spawn_aligned_player(reg, constants::PLAYER_Y);
    auto gate = spawn_obstacle(reg, ObstacleKind::ShapeGate, constants::PLAYER_Y);
    reg.emplace<RequiredShape>(gate, Shape::Circle);

    collision_system(reg, 0.016f);

    auto& gos = reg.ctx().get<GameOverState>();
    CHECK(gos.cause == DeathCause::MissedABeat);
}

TEST_CASE("redfoot/#168: energy depletion falls back to ENERGY DEPLETED",
          "[ui][redfoot][game_over][wiring]") {
    entt::registry reg;
    reg.ctx().emplace<GameState>().phase = GamePhase::Playing;
    auto& energy = reg.ctx().emplace<EnergyState>();
    auto& song = reg.ctx().emplace<SongState>();
    song.playing = true;
    reg.ctx().emplace<GameOverState>();  // cause stays None
    energy.energy = 0.0f;

    energy_system(reg, 0.016f);

    auto& gos = reg.ctx().get<GameOverState>();
    CHECK(gos.cause == DeathCause::EnergyDepleted);
}

TEST_CASE("redfoot/#168: energy depletion does not overwrite a specific cause",
          "[ui][redfoot][game_over][wiring]") {
    entt::registry reg;
    reg.ctx().emplace<GameState>().phase = GamePhase::Playing;
    auto& energy = reg.ctx().emplace<EnergyState>();
    auto& song = reg.ctx().emplace<SongState>();
    song.playing = true;
    auto& gos = reg.ctx().emplace<GameOverState>();
    gos.cause = DeathCause::HitABar;
    energy.energy = 0.0f;

    energy_system(reg, 0.016f);

    CHECK(gos.cause == DeathCause::HitABar);  // preserved, not clobbered
}
