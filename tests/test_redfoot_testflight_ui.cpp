// Tests for the Redfoot TestFlight UI pass — issues #168/#196 (game over
// reason) and #198 (settings toggles show state on the button face).
//
// These tests cover three layers:
//   * Resolver — the new `GameOverState.reason` source and the
//     `haptics_button` / `motion_button` formatters used by toggle buttons.
//   * Schema/content — shipped JSON has the new reason element on
//     game_over and the existing buttons sit at their original y_n.
//     Settings toggle buttons must declare a `text_source` so the face
//     reflects the runtime state.
//   * Wiring — collision and energy systems set `GameOverState.cause`
//     so the resolver returns the correct one-line reason.

#include <catch2/catch_test_macros.hpp>
#include <entt/entity/registry.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#include "components/game_state.h"
#include "components/obstacle.h"
#include "components/obstacle_data.h"
#include "components/player.h"
#include "components/rendering.h"
#include "components/rhythm.h"
#include "components/scoring.h"
#include "components/settings.h"
#include "components/song_state.h"
#include "components/transform.h"
#include "constants.h"
#include "systems/all_systems.h"
#include "systems/ui_source_resolver.h"

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

// ── Resolver: GameOverState.reason ──────────────────────────────────────────

TEST_CASE("redfoot/#168: GameOverState.reason resolves to platform-neutral copy",
          "[ui][redfoot][game_over]") {
    entt::registry reg;
    auto& gos = reg.ctx().emplace<GameOverState>();

    gos.cause = DeathCause::None;
    auto none_v = resolve_ui_dynamic_text(reg, "GameOverState.reason", "");
    REQUIRE(none_v.has_value());
    CHECK(none_v->empty());

    gos.cause = DeathCause::EnergyDepleted;
    CHECK(resolve_ui_dynamic_text(reg, "GameOverState.reason", "").value()
          == "ENERGY DEPLETED");

    gos.cause = DeathCause::MissedABeat;
    CHECK(resolve_ui_dynamic_text(reg, "GameOverState.reason", "").value()
          == "MISSED A BEAT");

    gos.cause = DeathCause::HitABar;
    CHECK(resolve_ui_dynamic_text(reg, "GameOverState.reason", "").value()
          == "HIT A BAR");
}

TEST_CASE("redfoot/#168: missing GameOverState yields nullopt", "[ui][redfoot]") {
    entt::registry reg;
    auto v = resolve_ui_dynamic_text(reg, "GameOverState.reason", "");
    CHECK_FALSE(v.has_value());
}

// ── Resolver: settings toggle button labels ─────────────────────────────────

TEST_CASE("redfoot/#198: haptics_button formatter renders state on button face",
          "[ui][redfoot][settings]") {
    entt::registry reg;
    auto& s = reg.ctx().emplace<SettingsState>();

    s.haptics_enabled = true;
    CHECK(resolve_ui_dynamic_text(reg,
            "SettingsState.haptics_enabled", "haptics_button").value()
          == "HAPTICS: ON");

    s.haptics_enabled = false;
    CHECK(resolve_ui_dynamic_text(reg,
            "SettingsState.haptics_enabled", "haptics_button").value()
          == "HAPTICS: OFF");
}

TEST_CASE("redfoot/#198: motion_button formatter renders state on button face",
          "[ui][redfoot][settings]") {
    entt::registry reg;
    auto& s = reg.ctx().emplace<SettingsState>();

    s.reduce_motion = true;
    CHECK(resolve_ui_dynamic_text(reg,
            "SettingsState.reduce_motion", "motion_button").value()
          == "MOTION: ON");

    s.reduce_motion = false;
    CHECK(resolve_ui_dynamic_text(reg,
            "SettingsState.reduce_motion", "motion_button").value()
          == "MOTION: OFF");
}

TEST_CASE("redfoot/#198: signed_ms formatter is unaffected by toggle changes",
          "[ui][redfoot][settings]") {
    entt::registry reg;
    auto& s = reg.ctx().emplace<SettingsState>();

    s.audio_offset_ms = 30;
    CHECK(resolve_ui_dynamic_text(reg,
            "SettingsState.audio_offset_ms", "signed_ms").value() == "+30 ms");

    s.audio_offset_ms = -40;
    CHECK(resolve_ui_dynamic_text(reg,
            "SettingsState.audio_offset_ms", "signed_ms").value() == "-40 ms");

    s.audio_offset_ms = 0;
    CHECK(resolve_ui_dynamic_text(reg,
            "SettingsState.audio_offset_ms", "signed_ms").value() == "+0 ms");
}

TEST_CASE("redfoot/#198: default integer formatter still works", "[ui][redfoot]") {
    entt::registry reg;
    auto& score = reg.ctx().emplace<ScoreState>();
    score.high_score = 12345;
    CHECK(resolve_ui_dynamic_text(reg, "ScoreState.high_score", "").value()
          == "12345");
}

// ── Content: game_over.json layout ──────────────────────────────────────────

TEST_CASE("redfoot/#168: game_over screen carries a one-line reason source",
          "[ui][redfoot][game_over]") {
    auto screen = load_screen("content/ui/screens/game_over.json");

    const json* reason = find_by_id(screen, "reason");
    REQUIRE(reason != nullptr);
    CHECK(reason->value("type", "") == "text_dynamic");
    CHECK(reason->value("source", "") == "GameOverState.reason");
    CHECK(reason->value("align", "") == "center");

    // Reason sits between the high score (y_n=0.438) and the first
    // button (y_n=0.6797) so it never visually crowds the buttons.
    const float y_n = reason->at("y_n").get<float>();
    CHECK(y_n > 0.438f);
    CHECK(y_n < 0.6797f);
}

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

// ── Content: settings.json toggle buttons show state on the face ────────────

TEST_CASE("redfoot/#198: settings toggle buttons declare a text_source",
          "[ui][redfoot][settings]") {
    auto screen = load_screen("content/ui/screens/settings.json");

    const json* haptics = find_by_id(screen, "haptics_toggle");
    const json* motion  = find_by_id(screen, "reduce_motion_toggle");
    REQUIRE(haptics != nullptr);
    REQUIRE(motion  != nullptr);

    CHECK(haptics->value("type", "") == "button");
    CHECK(haptics->value("text_source", "") == "SettingsState.haptics_enabled");
    CHECK(haptics->value("format", "") == "haptics_button");
    CHECK(haptics->value("action", "") == "haptics_toggle");

    CHECK(motion->value("type", "") == "button");
    CHECK(motion->value("text_source", "") == "SettingsState.reduce_motion");
    CHECK(motion->value("format", "") == "motion_button");
    CHECK(motion->value("action", "") == "reduce_motion_toggle");
}

TEST_CASE("redfoot/#198: audio offset controls remain pure -/+ buttons with display",
          "[ui][redfoot][settings]") {
    auto screen = load_screen("content/ui/screens/settings.json");

    const json* minus = find_by_id(screen, "audio_offset_minus");
    const json* plus  = find_by_id(screen, "audio_offset_plus");
    const json* disp  = find_by_id(screen, "audio_offset_display");
    REQUIRE(minus != nullptr);
    REQUIRE(plus  != nullptr);
    REQUIRE(disp  != nullptr);

    // -/+ buttons must NOT be turned into dynamic-text controls; they
    // remain literal +/- nudges and stay unchanged.
    CHECK_FALSE(minus->contains("text_source"));
    CHECK_FALSE(plus->contains("text_source"));
    CHECK(minus->value("text", "") == "-");
    CHECK(plus->value("text", "") == "+");

    // Audio offset display still uses signed_ms format.
    CHECK(disp->value("source", "") == "SettingsState.audio_offset_ms");
    CHECK(disp->value("format", "") == "signed_ms");
}

// ── Wiring: collision sets DeathCause for misses and bar hits ───────────────

namespace {
// Spawn a player aligned with an obstacle so the collision system's
// distance gate is satisfied; force a miss by leaving the player as
// Hexagon (matches no required shape, fails LowBar/HighBar action checks).
void spawn_aligned_player(entt::registry& reg, float y) {
    auto p = reg.create();
    reg.emplace<PlayerTag>(p);
    reg.emplace<Position>(p, constants::LANE_X[1], y);
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
    reg.emplace<Position>(e, constants::LANE_X[1], y);
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
    CHECK(resolve_ui_dynamic_text(reg, "GameOverState.reason", "").value()
          == "HIT A BAR");
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
    CHECK(resolve_ui_dynamic_text(reg, "GameOverState.reason", "").value()
          == "MISSED A BEAT");
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
    CHECK(resolve_ui_dynamic_text(reg, "GameOverState.reason", "").value()
          == "ENERGY DEPLETED");
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
