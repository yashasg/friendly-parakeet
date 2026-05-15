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
//   timing_tier == TimingTier::Good    → text "GOOD",     FontSize::Medium
//   timing_tier == TimingTier::Ok      → text "OK",       FontSize::Medium
//   timing_tier == TimingTier::Bad     → text "BAD",      FontSize::Medium
//   timing_tier == nullopt             → numeric score string (e.g. "150"), FontSize::Small
//   alpha at half lifetime → ≈ 127  (50 % × 255 truncated to uint8_t)
//
// The tests operate on in-memory registry entities and do not touch files.

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>
#include <cstring>
#include <optional>
#include <tuple>

#include "components/scoring.h"   // ScorePopup, PopupDisplay
#include "components/rendering.h" // ScreenPosition, Color (via raylib)
#include "systems/popup_display_system.h"
#include "components/text.h"      // FontSize
#include "components/rhythm.h"    // TimingTier
#include "components/transform.h" // WorldTransform, MotionVelocity
#include "entities/settings.h"        // SettingsState (reduce_motion)
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
    if (!reg.ctx().contains<PopupDisplayScratch>()) {
        runtime_system_scratch_init(reg);
    }
    auto e = reg.create();

    ScorePopup sp{};
    sp.has_timing_tier = timing_tier.has_value();
    sp.timing_tier = timing_tier.value_or(TimingTier::Ok);
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

TEST_CASE("popup_display_system: TimingTier::Good → GOOD, Medium font",
          "[popup_display][issue208][issue288]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, TimingTier::Good, 0, 1.0f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "GOOD") == 0);
    CHECK(pd.font_size == FontSize::Medium);
}

TEST_CASE("popup_display_system: TimingTier::Ok → OK, Medium font",
          "[popup_display][issue208][issue288]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, TimingTier::Ok, 0, 1.0f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "OK") == 0);
    CHECK(pd.font_size == FontSize::Medium);
}

TEST_CASE("popup_display_system: TimingTier::Bad → BAD, Medium font",
          "[popup_display][issue208][issue288]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, TimingTier::Bad, 0, 1.0f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "BAD") == 0);
    CHECK(pd.font_size == FontSize::Medium);
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

TEST_CASE("popup_display_system: expired popups are destroyed",
          "[popup_display]") {
    entt::registry reg;
    auto e = make_popup_entity(reg, std::nullopt, 50, 0.01f, 1.0f);

    popup_display_system(reg, 0.016f);

    CHECK_FALSE(reg.valid(e));
}

TEST_CASE("spawn_score_popup: creates the full popup display archetype",
          "[popup_display][archetype]") {
    entt::registry reg;
    auto e = spawn_score_popup(reg, {100.0f, 200.0f, 50, TimingTier::Good});

    REQUIRE(reg.all_of<ScorePopup, PopupDisplay, Color, MotionVelocity,
                       WorldTransform, DrawLayer, TagHUDPass>(e));
    CHECK(reg.get<ScorePopup>(e).has_timing_tier);
    CHECK(reg.get<ScorePopup>(e).timing_tier == TimingTier::Good);
    CHECK(std::strcmp(reg.get<PopupDisplay>(e).text, "GOOD") == 0);
}

TEST_CASE("init_popup_display: formats grade text + font size from ScorePopup (#251)",
          "[popup_display][issue251]") {
    ScorePopup sp{};
    sp.has_timing_tier = true;
    sp.timing_tier = TimingTier::Perfect;

    PopupDisplay pd{};
    init_popup_display(pd, sp, Color{10, 20, 30, 200});

    CHECK(std::strcmp(pd.text, "PERFECT") == 0);
    CHECK(pd.font_size == FontSize::Medium);
    CHECK(pd.r == 10);
    CHECK(pd.g == 20);
    CHECK(pd.b == 30);
    CHECK(pd.a == 200);
    CHECK(pd.text_half_width == 0.0f);
    CHECK(pd.measured_font_base_size == -1);
    CHECK(pd.measured_font_texture_id == 0u);
}

TEST_CASE("init_popup_display: nullopt tier formats numeric value (#251)",
          "[popup_display][issue251]") {
    ScorePopup sp{};
    sp.has_timing_tier = false;
    sp.value       = 4242;

    PopupDisplay pd{};
    init_popup_display(pd, sp, Color{255, 255, 255, 255});

    CHECK(std::strcmp(pd.text, "4242") == 0);
    CHECK(pd.font_size == FontSize::Small);
    CHECK(pd.text_half_width == 0.0f);
    CHECK(pd.measured_font_base_size == -1);
    CHECK(pd.measured_font_texture_id == 0u);
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

TEST_CASE("spawn_score_popup: ScorePopup has correct points and duration",
          "[popup_entity][issue349]") {
    entt::registry reg;
    auto e = spawn_score_popup(reg, {0.0f, 0.0f, 350, TimingTier::Good});

    REQUIRE(reg.all_of<ScorePopup>(e));
    const auto& sp = reg.get<ScorePopup>(e);
    CHECK(sp.value == 350);
    CHECK(sp.has_timing_tier);
    CHECK(sp.timing_tier == TimingTier::Good);
    CHECK(sp.remaining == constants::POPUP_DURATION);
    CHECK(sp.max_time  == constants::POPUP_DURATION);
}

TEST_CASE("spawn_score_popup: color is bright cyan for Perfect tier",
          "[popup_entity][issue386]") {
    entt::registry reg;
    auto e = spawn_score_popup(reg, {0.0f, 0.0f, 300, TimingTier::Perfect});

    REQUIRE(reg.all_of<Color>(e));
    const auto& c = reg.get<Color>(e);
    CHECK(c.r ==  80);
    CHECK(c.g == 255);
    CHECK(c.b == 220);
    CHECK(c.a == 255);
}

TEST_CASE("spawn_score_popup: Good tier is lime, Ok tier is amber, distinct from Perfect",
          "[popup_entity][issue386]") {
    entt::registry reg;
    auto good = spawn_score_popup(reg, {0.0f, 0.0f, 200, TimingTier::Good});
    auto ok   = spawn_score_popup(reg, {0.0f, 0.0f, 100, TimingTier::Ok});

    REQUIRE(reg.all_of<Color>(good));
    REQUIRE(reg.all_of<Color>(ok));

    const auto& good_color = reg.get<Color>(good);
    CHECK(good_color.r == 140);
    CHECK(good_color.g == 255);
    CHECK(good_color.b ==  80);

    const auto& ok_color = reg.get<Color>(ok);
    CHECK(ok_color.r == 255);
    CHECK(ok_color.g == 200);
    CHECK(ok_color.b ==  60);

    // Cross-tier distinctness (Perfect/Good/Ok must not collide).
    CHECK_FALSE((good_color.r == ok_color.r && good_color.g == ok_color.g && good_color.b == ok_color.b));
}

TEST_CASE("spawn_score_popup: color is red-orange for Bad tier",
          "[popup_entity][issue386]") {
    entt::registry reg;
    auto e = spawn_score_popup(reg, {0.0f, 0.0f, 50, TimingTier::Bad});

    REQUIRE(reg.all_of<Color>(e));
    const auto& c = reg.get<Color>(e);
    CHECK(c.r == 255);
    CHECK(c.g ==  90);
    CHECK(c.b ==  70);
}

TEST_CASE("spawn_score_popup: every timing tier maps to a distinct color",
          "[popup_entity][issue386]") {
    entt::registry reg;
    auto p = spawn_score_popup(reg, {0.0f, 0.0f, 0, TimingTier::Perfect});
    auto g = spawn_score_popup(reg, {0.0f, 0.0f, 0, TimingTier::Good});
    auto o = spawn_score_popup(reg, {0.0f, 0.0f, 0, TimingTier::Ok});
    auto b = spawn_score_popup(reg, {0.0f, 0.0f, 0, TimingTier::Bad});

    auto rgb = [&](entt::entity e) {
        const auto& c = reg.get<Color>(e);
        return std::tuple<uint8_t, uint8_t, uint8_t>{c.r, c.g, c.b};
    };
    auto cp = rgb(p), cg = rgb(g), co = rgb(o), cb = rgb(b);
    CHECK(cp != cg);
    CHECK(cp != co);
    CHECK(cp != cb);
    CHECK(cg != co);
    CHECK(cg != cb);
    CHECK(co != cb);
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

// ── #534: reduce_motion attenuates kinetic envelope ─────────────────────────

TEST_CASE("popup_display_system: reduce_motion zeroes drift velocity (#534)",
          "[popup_display][reduce_motion][issue534]") {
    entt::registry reg;
    runtime_system_scratch_init(reg);
    create_settings_entity(reg);
    settings_state(reg).reduce_motion = true;

    auto e = spawn_score_popup(reg, {100.0f, 500.0f, 200, TimingTier::Perfect});
    REQUIRE(reg.all_of<MotionVelocity>(e));
    REQUIRE(reg.get<MotionVelocity>(e).value.y == -80.0f);

    popup_display_system(reg, 0.016f);

    const auto& vel = reg.get<MotionVelocity>(e);
    CHECK(vel.value.x == 0.0f);
    CHECK(vel.value.y == 0.0f);

    // Informational channel (text/colour/value) is unchanged.
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "PERFECT") == 0);
    CHECK(pd.r ==  80);
    CHECK(pd.g == 255);
    CHECK(pd.b == 220);
    const auto& sp = reg.get<ScorePopup>(e);
    CHECK(sp.value == 200);
}

TEST_CASE("popup_display_system: reduce_motion=false leaves drift untouched (#534)",
          "[popup_display][reduce_motion][issue534]") {
    entt::registry reg;
    runtime_system_scratch_init(reg);
    create_settings_entity(reg);  // reduce_motion defaults to false

    auto e = spawn_score_popup(reg, {0.0f, 0.0f, 100, TimingTier::Good});
    popup_display_system(reg, 0.016f);

    const auto& vel = reg.get<MotionVelocity>(e);
    CHECK(vel.value.y == -80.0f);  // popup_display_system never touches it
}

// #1089 — PopupDisplayScratch::capacity_exceeded_count must stay at zero when
// a dense expiry pass fits inside the reserved expired buffer.
TEST_CASE("popup_display_system: dense expiry burst stays within reserved capacity",
          "[popup_display][issue1089]") {
    entt::registry reg;
    runtime_system_scratch_init(reg);
    constexpr int dense_count = 6;
    runtime_system_scratch_reserve(reg, dense_count);

    auto& scratch = reg.ctx().get<PopupDisplayScratch>();
    const auto expired_capacity = scratch.expired.capacity();

    for (int i = 0; i < dense_count; ++i) {
        // Tiny remaining lifetime so the next tick expires every popup.
        make_popup_entity(reg, std::nullopt, 100, 0.001f, 1.0f);
    }

    popup_display_system(reg, 0.016f);

    CHECK(scratch.expired.capacity() == expired_capacity);
    CHECK(scratch.capacity_exceeded_count == 0u);
}
