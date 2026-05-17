// Tests for gameplay HUD bind system (issues #1297, #1333).
//
// `gameplay_hud_bind_system` is a position-keyed bind for the dynamic-text
// HUD slots (`ScoreSlot`, `HighScoreSlot`, `ChainSlot`, `EnergyLabel`).
// It runs every gameplay frame; a regression in format strings, font
// sizing, alpha, or the `ChainBgPulseTag` insert/remove would silently
// break the in-game readout.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <string>

#include "test_helpers.h"
#include "components/scoring.h"
#include "components/ui.h"
#include "systems/gameplay_hud_bind_system.h"
#include "systems/generated/screen_spawners.h"
#include "tags/tags.h"

using Catch::Matchers::WithinAbs;

namespace {

constexpr float kScoreSlotX     =  60.0f, kScoreSlotY     = 20.0f;
constexpr float kHighScoreSlotX =  60.0f, kHighScoreSlotY = 50.0f;
constexpr float kChainSlotX     =  80.0f, kChainSlotY     = 80.0f;
constexpr float kEnergyLabelX   =  10.0f, kEnergyLabelY   = 745.0f;

bool pos_at(const UiPosition& pos, float x, float y) {
    return pos.x == x && pos.y == y;
}

entt::entity find_slot(entt::registry& reg, float x, float y) {
    auto view = reg.view<GameplayHudTag, UiLabelTag, UiPosition>();
    for (auto entity : view) {
        const auto& pos = view.get<UiPosition>(entity);
        if (pos_at(pos, x, y)) return entity;
    }
    return entt::null;
}

std::string label_text(const entt::registry& reg, entt::entity e) {
    const auto& label = reg.get<UiLabel>(e);
    return std::string(label.text.data());
}

}  // namespace

TEST_CASE("gameplay_hud_bind_system: ScoreSlot formats ScoreDisplay::displayed as plain integer",
          "[gameplay_hud][bind][issue1297][issue1333]") {
    auto reg = make_registry();
    spawn_gameplay_screen(reg);

    reg.ctx().get<ScoreDisplay>().displayed = 1234;
    gameplay_hud_bind_system(reg);

    const auto e = find_slot(reg, kScoreSlotX, kScoreSlotY);
    REQUIRE(reg.valid(e));

    CHECK(label_text(reg, e) == "1234");

    const auto* font = reg.try_get<UiLabelFontSize>(e);
    REQUIRE(font != nullptr);
    CHECK(font->size == 28);

    const auto* alpha = reg.try_get<UiLabelAlpha>(e);
    REQUIRE(alpha != nullptr);
    CHECK_THAT(alpha->value, WithinAbs(1.0f, 0.0001f));
}

TEST_CASE("gameplay_hud_bind_system: HighScoreSlot formats CurrentSongHighScore as 'BEST: %d'",
          "[gameplay_hud][bind][issue1297][issue1333]") {
    auto reg = make_registry();
    spawn_gameplay_screen(reg);

    reg.ctx().get<CurrentSongHighScore>().value = 5678;
    gameplay_hud_bind_system(reg);

    const auto e = find_slot(reg, kHighScoreSlotX, kHighScoreSlotY);
    REQUIRE(reg.valid(e));

    CHECK(label_text(reg, e) == "BEST: 5678");

    const auto* font = reg.try_get<UiLabelFontSize>(e);
    REQUIRE(font != nullptr);
    CHECK(font->size == 18);

    const auto* alpha = reg.try_get<UiLabelAlpha>(e);
    REQUIRE(alpha != nullptr);
    CHECK_THAT(alpha->value, WithinAbs(0.7f, 0.0001f));
}

TEST_CASE("gameplay_hud_bind_system: ChainSlot is empty and ChainBgPulseTag absent when chain < 2",
          "[gameplay_hud][bind][issue1297][issue1333]") {
    auto reg = make_registry();
    spawn_gameplay_screen(reg);

    reg.ctx().get<ScoreState>().chain_count = 1;
    gameplay_hud_bind_system(reg);

    const auto e = find_slot(reg, kChainSlotX, kChainSlotY);
    REQUIRE(reg.valid(e));

    CHECK(label_text(reg, e) == "");
    CHECK_FALSE(reg.all_of<ChainBgPulseTag>(e));

    // chain_count == 0 path also drops the tag (and label).
    reg.ctx().get<ScoreState>().chain_count = 0;
    reg.emplace<ChainBgPulseTag>(e);  // simulate a stale halo from a prior frame
    gameplay_hud_bind_system(reg);
    CHECK(label_text(reg, e) == "");
    CHECK_FALSE(reg.all_of<ChainBgPulseTag>(e));
}

TEST_CASE("gameplay_hud_bind_system: ChainSlot shows 'CHAIN N' below the meaningful threshold without the halo",
          "[gameplay_hud][bind][issue1297][issue1333]") {
    auto reg = make_registry();
    spawn_gameplay_screen(reg);

    reg.ctx().get<ScoreState>().chain_count = 2;
    gameplay_hud_bind_system(reg);

    const auto e = find_slot(reg, kChainSlotX, kChainSlotY);
    REQUIRE(reg.valid(e));

    CHECK(label_text(reg, e) == "CHAIN 2");
    CHECK_FALSE(reg.all_of<ChainBgPulseTag>(e));

    const auto* font = reg.try_get<UiLabelFontSize>(e);
    REQUIRE(font != nullptr);
    CHECK(font->size == 18);

    const auto* alpha = reg.try_get<UiLabelAlpha>(e);
    REQUIRE(alpha != nullptr);
    CHECK_THAT(alpha->value, WithinAbs(0.7f, 0.0001f));
}

TEST_CASE("gameplay_hud_bind_system: ChainSlot adds '!' and ChainBgPulseTag at the meaningful threshold (chain >= 5)",
          "[gameplay_hud][bind][issue1297][issue1333]") {
    auto reg = make_registry();
    spawn_gameplay_screen(reg);

    reg.ctx().get<ScoreState>().chain_count = 5;
    gameplay_hud_bind_system(reg);

    const auto e = find_slot(reg, kChainSlotX, kChainSlotY);
    REQUIRE(reg.valid(e));

    CHECK(label_text(reg, e) == "CHAIN 5!");
    CHECK(reg.all_of<ChainBgPulseTag>(e));

    const auto* font = reg.try_get<UiLabelFontSize>(e);
    REQUIRE(font != nullptr);
    CHECK(font->size == 20);

    const auto* alpha = reg.try_get<UiLabelAlpha>(e);
    REQUIRE(alpha != nullptr);
    CHECK_THAT(alpha->value, WithinAbs(0.95f, 0.0001f));
}

TEST_CASE("gameplay_hud_bind_system: ChainBgPulseTag is removed when chain falls back below threshold",
          "[gameplay_hud][bind][issue1297][issue1333]") {
    auto reg = make_registry();
    spawn_gameplay_screen(reg);

    // Streak peaks: emplace the halo.
    reg.ctx().get<ScoreState>().chain_count = 7;
    gameplay_hud_bind_system(reg);
    const auto e = find_slot(reg, kChainSlotX, kChainSlotY);
    REQUIRE(reg.valid(e));
    CHECK(reg.all_of<ChainBgPulseTag>(e));

    // Streak drops (still visible, no halo).
    reg.ctx().get<ScoreState>().chain_count = 3;
    gameplay_hud_bind_system(reg);
    CHECK(label_text(reg, e) == "CHAIN 3");
    CHECK_FALSE(reg.all_of<ChainBgPulseTag>(e));

    // Streak breaks (cleared, no halo).
    reg.ctx().get<ScoreState>().chain_count = 0;
    gameplay_hud_bind_system(reg);
    CHECK(label_text(reg, e) == "");
    CHECK_FALSE(reg.all_of<ChainBgPulseTag>(e));
}

TEST_CASE("gameplay_hud_bind_system: EnergyLabel keeps its static text and gets per-slot font/alpha",
          "[gameplay_hud][bind][issue1297][issue1333]") {
    auto reg = make_registry();
    spawn_gameplay_screen(reg);

    gameplay_hud_bind_system(reg);

    const auto e = find_slot(reg, kEnergyLabelX, kEnergyLabelY);
    REQUIRE(reg.valid(e));

    // Static label is set at spawn by the codegen — bind must NOT clobber it.
    CHECK(label_text(reg, e) == "ENERGY");

    const auto* font = reg.try_get<UiLabelFontSize>(e);
    REQUIRE(font != nullptr);
    CHECK(font->size == 16);

    const auto* alpha = reg.try_get<UiLabelAlpha>(e);
    REQUIRE(alpha != nullptr);
    CHECK_THAT(alpha->value, WithinAbs(0.8f, 0.0001f));
}

TEST_CASE("gameplay_hud_bind_system: no gameplay-HUD entities is a safe no-op",
          "[gameplay_hud][bind][issue1297][issue1333]") {
    auto reg = make_registry();
    // Intentionally don't spawn the gameplay screen.
    reg.ctx().get<ScoreDisplay>().displayed = 999;
    reg.ctx().get<ScoreState>().chain_count = 7;
    reg.ctx().get<CurrentSongHighScore>().value = 42;

    REQUIRE_NOTHROW(gameplay_hud_bind_system(reg));
    CHECK(reg.view<GameplayHudTag>().size() == 0);
}
