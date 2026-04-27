// Regression tests for GitHub issue #288 (originally filed as #208).
//
// Covers popup_display_system — the system that converts ScorePopup entities
// into renderable PopupDisplay structs (text label, font size, RGBA alpha).
//
// Test matrix:
//   timing_tier == TimingTier::Perfect → text "PERFECT",  FontSize::Medium
//   timing_tier == TimingTier::Good    → text "GOOD",     FontSize::Small
//   timing_tier == TimingTier::Ok      → text "OK",       FontSize::Small
//   timing_tier == TimingTier::Bad     → text "BAD",      FontSize::Small
//   timing_tier == nullopt             → numeric score string (e.g. "150"), FontSize::Small
//   alpha at half lifetime → ≈ 127  (50 % × 255 truncated to uint8_t)
//
// The tests operate on in-memory registry entities and do not touch files.

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>
#include <cstring>
#include <optional>

#include "components/scoring.h"   // ScorePopup, PopupDisplay
#include "components/rendering.h" // ScreenPosition, Color (via raylib)
#include "components/lifetime.h"  // Lifetime
#include "components/text.h"      // FontSize
#include "components/rhythm.h"    // TimingTier
#include "systems/all_systems.h"  // popup_display_system declaration

// ── Helper ────────────────────────────────────────────────────────────────
//
// Build a minimal entity that popup_display_system can process.

static entt::entity make_popup_entity(entt::registry& reg,
                                      std::optional<TimingTier> timing_tier,
                                      int32_t value,
                                      float   remaining,
                                      float   max_time) {
    auto e = reg.create();

    ScorePopup sp{};
    sp.timing_tier = timing_tier;
    sp.value       = value;
    reg.emplace<ScorePopup>(e, sp);

    reg.emplace<ScreenPosition>(e, 0.0f, 0.0f);
    reg.emplace<Color>(e, Color{255, 255, 255, 255});

    Lifetime lt{};
    lt.remaining = remaining;
    lt.max_time  = max_time;
    reg.emplace<Lifetime>(e, lt);

    return e;
}

// ── Grade text + font size ────────────────────────────────────────────────

TEST_CASE("popup_display_system: TimingTier::Perfect → PERFECT, Medium font",
          "[popup_display][issue288]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, TimingTier::Perfect, 0, 1.0f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "PERFECT") == 0);
    CHECK(pd.font_size == FontSize::Medium);
}

TEST_CASE("popup_display_system: TimingTier::Good → GOOD, Small font",
          "[popup_display][issue288]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, TimingTier::Good, 0, 1.0f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "GOOD") == 0);
    CHECK(pd.font_size == FontSize::Small);
}

TEST_CASE("popup_display_system: TimingTier::Ok → OK, Small font",
          "[popup_display][issue288]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, TimingTier::Ok, 0, 1.0f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "OK") == 0);
    CHECK(pd.font_size == FontSize::Small);
}

TEST_CASE("popup_display_system: TimingTier::Bad → BAD, Small font",
          "[popup_display][issue288]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, TimingTier::Bad, 0, 1.0f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "BAD") == 0);
    CHECK(pd.font_size == FontSize::Small);
}

// ── Numeric score path ────────────────────────────────────────────────────

TEST_CASE("popup_display_system: nullopt timing_tier → numeric score string",
          "[popup_display][issue288]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, std::nullopt, 150, 1.0f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    // Non-shape obstacle path: value formatted as decimal string.
    CHECK(std::strcmp(pd.text, "150") == 0);
    CHECK(pd.font_size == FontSize::Small);
}

// ── Alpha decay ───────────────────────────────────────────────────────────

TEST_CASE("popup_display_system: alpha at half lifetime is 127",
          "[popup_display][issue288]") {
    entt::registry reg;
    // remaining = 0.5, max_time = 1.0 → alpha_ratio = 0.5
    // pd.a = static_cast<uint8_t>(0.5f * 255) = 127
    auto e = make_popup_entity(reg, TimingTier::Perfect, 0, 0.5f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(pd.a == uint8_t{127});
}
