// Regression tests for GitHub issues #208 and #288.
//
// #208: popup_display_system had zero test coverage.
// #288: timing_tier migrated from raw uint8_t sentinel (255) to optional<TimingTier>.
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
#include "components/text.h"      // FontSize
#include "components/rhythm.h"    // TimingTier
#include "components/transform.h" // WorldTransform, MotionVelocity
#include "systems/all_systems.h"  // popup_display_system declaration
#include "entities/popup_entity.h"
#include "constants.h"

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
    sp.remaining   = remaining;
    sp.max_time    = max_time;
    reg.emplace<ScorePopup>(e, sp);

    reg.emplace<ScreenPosition>(e, 0.0f, 0.0f);
    reg.emplace<Color>(e, Color{255, 255, 255, 255});

    // #251: PopupDisplay's static fields (text, font size, base RGB) are
    // initialized once at spawn — popup_display_system only fades alpha.
    PopupDisplay pd{};
    init_popup_display(pd, sp, Color{255, 255, 255, 255});
    reg.emplace<PopupDisplay>(e, pd);

    return e;
}

// ── Grade text + font size ────────────────────────────────────────────────

TEST_CASE("popup_display_system: TimingTier::Perfect → PERFECT, Medium font",
          "[popup_display][issue208][issue288]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, TimingTier::Perfect, 0, 1.0f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "PERFECT") == 0);
    CHECK(pd.font_size == FontSize::Medium);
}

TEST_CASE("popup_display_system: TimingTier::Good → GOOD, Small font",
          "[popup_display][issue208][issue288]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, TimingTier::Good, 0, 1.0f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "GOOD") == 0);
    CHECK(pd.font_size == FontSize::Small);
}

TEST_CASE("popup_display_system: TimingTier::Ok → OK, Small font",
          "[popup_display][issue208][issue288]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, TimingTier::Ok, 0, 1.0f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "OK") == 0);
    CHECK(pd.font_size == FontSize::Small);
}

TEST_CASE("popup_display_system: TimingTier::Bad → BAD, Small font",
          "[popup_display][issue208][issue288]") {
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
          "[popup_display][issue208][issue288]") {
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
          "[popup_display][issue208][issue288]") {
    entt::registry reg;
    // remaining = 0.5, max_time = 1.0 → alpha_ratio = 0.5
    // pd.a = static_cast<uint8_t>(0.5f * 255) = 127
    auto e = make_popup_entity(reg, TimingTier::Perfect, 0, 0.5f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(pd.a == uint8_t{127});
}


// ── #251: static formatting is one-shot ───────────────────────────────────
//
// popup_display_system used to re-snprintf the text and emplace_or_replace
// PopupDisplay every tick. After #251 it must only mutate the per-frame
// alpha; text, font size, and base RGB are written once at spawn via
// init_popup_display(). These tests would fail if the system reverted to
// re-formatting on every call.

TEST_CASE("popup_display_system: does not re-format static text per tick (#251)",
          "[popup_display][issue251]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, TimingTier::Perfect, 0, 1.0f, 1.0f);

    auto& pd_mut = reg.get<PopupDisplay>(e);
    std::snprintf(pd_mut.text, sizeof(pd_mut.text), "%s", "SENTINEL");
    pd_mut.font_size = FontSize::Large;

    popup_display_system(reg, 0.0f);
    popup_display_system(reg, 0.0f);
    popup_display_system(reg, 0.0f);

    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "SENTINEL") == 0);
    CHECK(pd.font_size == FontSize::Large);
}

TEST_CASE("popup_display_system: does not churn PopupDisplay storage (#251)",
          "[popup_display][issue251]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, TimingTier::Good, 0, 1.0f, 1.0f);

    auto& storage = reg.storage<PopupDisplay>();
    const auto sz0 = storage.size();
    REQUIRE(storage.contains(e));

    popup_display_system(reg, 0.0f);
    popup_display_system(reg, 0.0f);

    CHECK(storage.size() == sz0);
    CHECK(storage.contains(e));
}

TEST_CASE("popup_display_system: works without ScorePopup component (#251)",
           "[popup_display][issue251]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, TimingTier::Ok, 0, 0.5f, 1.0f);
    reg.remove<ScorePopup>(e);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(pd.a == uint8_t{255});
    CHECK(std::strcmp(pd.text, "OK") == 0);
}

TEST_CASE("popup_display_system: ScorePopup expires without render components",
          "[popup_display]") {
    entt::registry reg;
    auto e = reg.create();
    ScorePopup popup{};
    popup.remaining = 0.01f;
    popup.max_time = 1.0f;
    reg.emplace<ScorePopup>(e, popup);

    popup_display_system(reg, 0.02f);

    CHECK_FALSE(reg.valid(e));
}

TEST_CASE("popup_display_system: expired popups are destroyed",
          "[popup_display]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, std::nullopt, 50, 0.01f, 1.0f);

    popup_display_system(reg, 0.016f);

    CHECK_FALSE(reg.valid(e));
}

TEST_CASE("init_popup_display: formats grade text + font size from ScorePopup (#251)",
          "[popup_display][issue251]") {
    ScorePopup sp{};
    sp.timing_tier = TimingTier::Perfect;

    PopupDisplay pd{};
    init_popup_display(pd, sp, Color{10, 20, 30, 200});

    CHECK(std::strcmp(pd.text, "PERFECT") == 0);
    CHECK(pd.font_size == FontSize::Medium);
    CHECK(pd.r == 10);
    CHECK(pd.g == 20);
    CHECK(pd.b == 30);
    CHECK(pd.a == 200);
}

TEST_CASE("init_popup_display: nullopt tier formats numeric value (#251)",
          "[popup_display][issue251]") {
    ScorePopup sp{};
    sp.timing_tier = std::nullopt;
    sp.value       = 4242;

    PopupDisplay pd{};
    init_popup_display(pd, sp, Color{255, 255, 255, 255});

    CHECK(std::strcmp(pd.text, "4242") == 0);
    CHECK(pd.font_size == FontSize::Small);
}

// ── spawn_score_popup: entity factory contract (#349) ─────────────────────────
//
// These tests prove that spawn_score_popup creates an entity carrying the full
// expected component bundle with correct values.

TEST_CASE("spawn_score_popup: entity has WorldTransform at pos - 40",
          "[popup_entity][issue349]") {
    entt::registry reg;
    auto e = spawn_score_popup(reg, {100.0f, 500.0f, 200, std::nullopt});

    REQUIRE(reg.all_of<WorldTransform>(e));
    const auto& wt = reg.get<WorldTransform>(e);
    CHECK(wt.position.x == 100.0f);
    CHECK(wt.position.y == 460.0f);  // 500 - 40
}

TEST_CASE("spawn_score_popup: entity has MotionVelocity {0, -80}",
          "[popup_entity][issue349]") {
    entt::registry reg;
    auto e = spawn_score_popup(reg, {0.0f, 0.0f, 100, std::nullopt});

    REQUIRE(reg.all_of<MotionVelocity>(e));
    const auto& mv = reg.get<MotionVelocity>(e);
    CHECK(mv.value.x == 0.0f);
    CHECK(mv.value.y == -80.0f);
}

TEST_CASE("spawn_score_popup: ScorePopup has correct points, tier=0, duration",
          "[popup_entity][issue349]") {
    entt::registry reg;
    auto e = spawn_score_popup(reg, {0.0f, 0.0f, 350, TimingTier::Good});

    REQUIRE(reg.all_of<ScorePopup>(e));
    const auto& sp = reg.get<ScorePopup>(e);
    CHECK(sp.value == 350);
    CHECK(sp.tier == uint8_t{0});
    CHECK(sp.timing_tier == TimingTier::Good);
    CHECK(sp.remaining == constants::POPUP_DURATION);
    CHECK(sp.max_time  == constants::POPUP_DURATION);
}

TEST_CASE("spawn_score_popup: color is green for Perfect tier",
          "[popup_entity][issue349]") {
    entt::registry reg;
    auto e = spawn_score_popup(reg, {0.0f, 0.0f, 300, TimingTier::Perfect});

    REQUIRE(reg.all_of<Color>(e));
    const auto& c = reg.get<Color>(e);
    CHECK(c.r == 100);
    CHECK(c.g == 255);
    CHECK(c.b == 100);
    CHECK(c.a == 255);
}

TEST_CASE("spawn_score_popup: color is orange for Bad tier",
          "[popup_entity][issue349]") {
    entt::registry reg;
    auto e = spawn_score_popup(reg, {0.0f, 0.0f, 50, TimingTier::Bad});

    REQUIRE(reg.all_of<Color>(e));
    const auto& c = reg.get<Color>(e);
    CHECK(c.r == 255);
    CHECK(c.g == 150);
    CHECK(c.b == 100);
}

TEST_CASE("spawn_score_popup: default color when no timing tier",
          "[popup_entity][issue349]") {
    entt::registry reg;
    auto e = spawn_score_popup(reg, {0.0f, 0.0f, 200, std::nullopt});

    REQUIRE(reg.all_of<Color>(e));
    const auto& c = reg.get<Color>(e);
    CHECK(c.r == 255);
    CHECK(c.g == 255);
    CHECK(c.b == 50);
}

TEST_CASE("spawn_score_popup: DrawLayer is Effects",
          "[popup_entity][issue349]") {
    entt::registry reg;
    auto e = spawn_score_popup(reg, {0.0f, 0.0f, 100, std::nullopt});

    REQUIRE(reg.all_of<DrawLayer>(e));
    CHECK(reg.get<DrawLayer>(e).layer == Layer::Effects);
}

TEST_CASE("spawn_score_popup: entity carries TagHUDPass",
          "[popup_entity][issue349]") {
    entt::registry reg;
    auto e = spawn_score_popup(reg, {0.0f, 0.0f, 100, std::nullopt});
    CHECK(reg.all_of<TagHUDPass>(e));
}

TEST_CASE("spawn_score_popup: PopupDisplay initialized at spawn",
          "[popup_entity][issue349]") {
    entt::registry reg;
    auto e = spawn_score_popup(reg, {0.0f, 0.0f, 0, TimingTier::Perfect});

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "PERFECT") == 0);
    CHECK(pd.font_size == FontSize::Medium);
    CHECK(pd.a == 255);
}
