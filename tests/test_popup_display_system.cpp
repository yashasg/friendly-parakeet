// Regression tests for GitHub issues #208, #288, #1202.
//
// #208: popup_display_system had zero test coverage.
// #288: timing_tier migrated from raw uint8_t sentinel (255) to optional<TimingTier>.
// #1202: TimingTier enum eradicated (Fabian existential processing). Each
//        former tier is now an empty tag on the popup entity itself
//        (TimingPerfectTag / TimingGoodTag / TimingOkTag / TimingBadTag).
//        The discriminator field on ScorePopup is gone — tier identity is
//        carried by the popup entity's component signature.
//
// Covers popup_display_system — the system that converts ScorePopup entities
// into renderable PopupDisplay structs (text label, font size, RGBA alpha).
//
// Test matrix (post-#1202):
//   TimingPerfectTag on entity → text "PERFECT",  FontSize::Medium
//   TimingGoodTag    on entity → text "GOOD",     FontSize::Medium
//   TimingOkTag      on entity → text "OK",       FontSize::Medium
//   TimingBadTag     on entity → text "BAD",      FontSize::Medium
//   no tier tag                → numeric score string (e.g. "150"), FontSize::Small
//   alpha at half lifetime → ≈ 127  (50 % × 255 truncated to uint8_t)
//
// The tests operate on in-memory registry entities and do not touch files.

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>
#include <cstring>
#include <tuple>

#include "components/scoring.h"   // ScorePopup, PopupDisplay
#include "components/rendering.h" // ScreenPosition, Color (via raylib)
#include "components/text.h"      // FontSize
#include "tags/tags.h"            // TimingPerfectTag / Good / Ok / Bad
#include "components/transform.h" // WorldPosition, Vector2
#include "entities/settings.h"    // SettingsState (reduce_motion)
#include "systems/all_systems.h"  // popup_display_system declaration
#include "entities/popup_entity.h"
#include "constants.h"

// ── Helpers ───────────────────────────────────────────────────────────────
//
// Build a minimal popup entity that popup_display_system can process. One
// helper per former TimingTier value (existential processing — no enum).

namespace {

void seed_popup_common(entt::registry& reg,
                       entt::entity   e,
                       int32_t        value,
                       float          remaining,
                       float          max_time) {
    // Idempotent / lightweight after issue #1626 (ScorePopupRequestQueue
    // eradicated into per-tier row tables); always call so popup_display
    // tests don't depend on a now-defunct ctx sentinel.
    runtime_system_scratch_init(reg);
    ScorePopup sp{};
    sp.value     = value;
    sp.remaining = remaining;
    sp.max_time  = max_time;
    reg.emplace<ScorePopup>(e, sp);

    reg.emplace<ScreenPosition>(e, 0.0f, 0.0f);
    reg.emplace<Color>(e, Color{255, 255, 255, 255});
}

entt::entity make_perfect_popup(entt::registry& reg, int32_t v, float rem, float max) {
    auto e = reg.create();
    seed_popup_common(reg, e, v, rem, max);
    reg.emplace<TimingPerfectTag>(e);
    PopupDisplay pd{};
    init_popup_display_perfect(pd, Color{255, 255, 255, 255});
    reg.emplace<PopupDisplay>(e, pd);
    return e;
}

entt::entity make_good_popup(entt::registry& reg, int32_t v, float rem, float max) {
    auto e = reg.create();
    seed_popup_common(reg, e, v, rem, max);
    reg.emplace<TimingGoodTag>(e);
    PopupDisplay pd{};
    init_popup_display_good(pd, Color{255, 255, 255, 255});
    reg.emplace<PopupDisplay>(e, pd);
    return e;
}

entt::entity make_ok_popup(entt::registry& reg, int32_t v, float rem, float max) {
    auto e = reg.create();
    seed_popup_common(reg, e, v, rem, max);
    reg.emplace<TimingOkTag>(e);
    PopupDisplay pd{};
    init_popup_display_ok(pd, Color{255, 255, 255, 255});
    reg.emplace<PopupDisplay>(e, pd);
    return e;
}

entt::entity make_bad_popup(entt::registry& reg, int32_t v, float rem, float max) {
    auto e = reg.create();
    seed_popup_common(reg, e, v, rem, max);
    reg.emplace<TimingBadTag>(e);
    PopupDisplay pd{};
    init_popup_display_bad(pd, Color{255, 255, 255, 255});
    reg.emplace<PopupDisplay>(e, pd);
    return e;
}

entt::entity make_untimed_popup(entt::registry& reg, int32_t v, float rem, float max) {
    auto e = reg.create();
    seed_popup_common(reg, e, v, rem, max);
    PopupDisplay pd{};
    const auto& sp = reg.get<ScorePopup>(e);
    init_popup_display_untimed(pd, sp, Color{255, 255, 255, 255});
    reg.emplace<PopupDisplay>(e, pd);
    return e;
}

}  // namespace

// ── Grade text + font size ────────────────────────────────────────────────

TEST_CASE("popup_display_system: TimingPerfectTag → PERFECT, Medium font",
          "[popup_display][issue208][issue288][issue1202]") {
    entt::registry reg;
    auto e = make_perfect_popup(reg, 0, 1.0f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "PERFECT") == 0);
    CHECK(pd.font_size == FontSize::Medium);
}

TEST_CASE("popup_display_system: TimingGoodTag → GOOD, Medium font",
          "[popup_display][issue208][issue288][issue1202]") {
    entt::registry reg;
    auto e = make_good_popup(reg, 0, 1.0f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "GOOD") == 0);
    CHECK(pd.font_size == FontSize::Medium);
}

TEST_CASE("popup_display_system: TimingOkTag → OK, Medium font",
          "[popup_display][issue208][issue288][issue1202]") {
    entt::registry reg;
    auto e = make_ok_popup(reg, 0, 1.0f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "OK") == 0);
    CHECK(pd.font_size == FontSize::Medium);
}

TEST_CASE("popup_display_system: TimingBadTag → BAD, Medium font",
          "[popup_display][issue208][issue288][issue1202]") {
    entt::registry reg;
    auto e = make_bad_popup(reg, 0, 1.0f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(std::strcmp(pd.text, "BAD") == 0);
    CHECK(pd.font_size == FontSize::Medium);
}

// ── Numeric score path ────────────────────────────────────────────────────

TEST_CASE("popup_display_system: untimed (no tier tag) → numeric score string",
          "[popup_display][issue208][issue288][issue1202]") {
    entt::registry reg;
    auto e = make_untimed_popup(reg, 150, 1.0f, 1.0f);

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
    auto e = make_perfect_popup(reg, 0, 0.5f, 1.0f);

    popup_display_system(reg, 0.0f);

    REQUIRE(reg.all_of<PopupDisplay>(e));
    const auto& pd = reg.get<PopupDisplay>(e);
    CHECK(pd.a == uint8_t{127});
}


// ── #251: static formatting is one-shot ───────────────────────────────────
//
// popup_display_system used to re-snprintf the text and emplace_or_replace
// PopupDisplay every tick. After #251 it must only mutate the per-frame
// alpha; text, font size, and base RGB are written once at spawn via the
// per-tier init_popup_display_* helpers (post-#1202). These tests would fail
// if the system reverted to re-formatting on every call.

TEST_CASE("popup_display_system: does not re-format static text per tick (#251)",
          "[popup_display][issue251]") {
    entt::registry reg;
    auto e = make_perfect_popup(reg, 0, 1.0f, 1.0f);

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
    auto e = make_good_popup(reg, 0, 1.0f, 1.0f);

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
    auto e = make_ok_popup(reg, 0, 0.5f, 1.0f);
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
    auto e = make_untimed_popup(reg, 50, 0.01f, 1.0f);

    popup_display_system(reg, 0.016f);

    CHECK_FALSE(reg.valid(e));
}

TEST_CASE("spawn_score_popup_good: creates the full popup display archetype",
          "[popup_display][archetype][issue1202]") {
    entt::registry reg;
    auto e = spawn_score_popup_good(reg, 100.0f, 200.0f, 50);

    REQUIRE(reg.all_of<ScorePopup, PopupDisplay, Color, Vector2,
                       WorldPosition, TagHUDPass>(e));
    CHECK(reg.all_of<TimingGoodTag>(e));
    CHECK(std::strcmp(reg.get<PopupDisplay>(e).text, "GOOD") == 0);
}

TEST_CASE("init_popup_display_perfect: formats PERFECT text + Medium font (#251)",
          "[popup_display][issue251][issue1202]") {
    PopupDisplay pd{};
    init_popup_display_perfect(pd, Color{10, 20, 30, 200});

    CHECK(std::strcmp(pd.text, "PERFECT") == 0);
    CHECK(pd.font_size == FontSize::Medium);
    CHECK(pd.r == 10);
    CHECK(pd.g == 20);
    CHECK(pd.b == 30);
    CHECK(pd.a == 200);
}

TEST_CASE("init_popup_display_untimed: formats numeric value (#251)",
          "[popup_display][issue251][issue1202]") {
    ScorePopup sp{};
    sp.value = 4242;

    PopupDisplay pd{};
    init_popup_display_untimed(pd, sp, Color{255, 255, 255, 255});

    CHECK(std::strcmp(pd.text, "4242") == 0);
    CHECK(pd.font_size == FontSize::Small);
}

// ── per-tier spawn_score_popup_*: entity factory contract (#349, #1202) ──────
//
// These tests prove that the per-tier spawn functions create an entity carrying
// the full expected component bundle with correct values plus the matching
// per-tier tag.

TEST_CASE("spawn_score_popup_untimed: entity has WorldPosition at pos - 40",
          "[popup_entity][issue349]") {
    entt::registry reg;
    auto e = spawn_score_popup_untimed(reg, 100.0f, 500.0f, 200);

    REQUIRE(reg.all_of<WorldPosition>(e));
    const auto& wt = reg.get<WorldPosition>(e);
    CHECK(wt.position.x == 100.0f);
    CHECK(wt.position.y == 460.0f);  // 500 - 40
}

TEST_CASE("spawn_score_popup_untimed: entity has Vector2 {0, -80}",
          "[popup_entity][issue349]") {
    entt::registry reg;
    auto e = spawn_score_popup_untimed(reg, 0.0f, 0.0f, 100);

    REQUIRE(reg.all_of<Vector2>(e));
    const auto& mv = reg.get<Vector2>(e);
    CHECK(mv.x == 0.0f);
    CHECK(mv.y == -80.0f);
}

TEST_CASE("spawn_score_popup_good: ScorePopup has correct points and duration, tag emplaced",
          "[popup_entity][issue349][issue1202]") {
    entt::registry reg;
    auto e = spawn_score_popup_good(reg, 0.0f, 0.0f, 350);

    REQUIRE(reg.all_of<ScorePopup>(e));
    const auto& sp = reg.get<ScorePopup>(e);
    CHECK(sp.value == 350);
    CHECK(reg.all_of<TimingGoodTag>(e));
    CHECK(sp.remaining == constants::POPUP_DURATION);
    CHECK(sp.max_time  == constants::POPUP_DURATION);
}

TEST_CASE("spawn_score_popup_perfect: color is bright cyan",
          "[popup_entity][issue386]") {
    entt::registry reg;
    auto e = spawn_score_popup_perfect(reg, 0.0f, 0.0f, 300);

    REQUIRE(reg.all_of<Color>(e));
    const auto& c = reg.get<Color>(e);
    CHECK(c.r ==  80);
    CHECK(c.g == 255);
    CHECK(c.b == 220);
    CHECK(c.a == 255);
}

TEST_CASE("spawn_score_popup_good: lime; spawn_score_popup_ok: amber; distinct from Perfect",
          "[popup_entity][issue386]") {
    entt::registry reg;
    auto good = spawn_score_popup_good(reg, 0.0f, 0.0f, 200);
    auto ok   = spawn_score_popup_ok(reg,   0.0f, 0.0f, 100);

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

    CHECK_FALSE((good_color.r == ok_color.r && good_color.g == ok_color.g && good_color.b == ok_color.b));
}

TEST_CASE("spawn_score_popup_bad: color is red-orange",
          "[popup_entity][issue386]") {
    entt::registry reg;
    auto e = spawn_score_popup_bad(reg, 0.0f, 0.0f, 50);

    REQUIRE(reg.all_of<Color>(e));
    const auto& c = reg.get<Color>(e);
    CHECK(c.r == 255);
    CHECK(c.g ==  90);
    CHECK(c.b ==  70);
}

TEST_CASE("spawn_score_popup_*: every timing tier maps to a distinct color",
          "[popup_entity][issue386]") {
    entt::registry reg;
    auto p = spawn_score_popup_perfect(reg, 0.0f, 0.0f, 0);
    auto g = spawn_score_popup_good   (reg, 0.0f, 0.0f, 0);
    auto o = spawn_score_popup_ok     (reg, 0.0f, 0.0f, 0);
    auto b = spawn_score_popup_bad    (reg, 0.0f, 0.0f, 0);

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

TEST_CASE("spawn_score_popup_untimed: default color when no timing tier",
          "[popup_entity][issue349]") {
    entt::registry reg;
    auto e = spawn_score_popup_untimed(reg, 0.0f, 0.0f, 200);

    REQUIRE(reg.all_of<Color>(e));
    const auto& c = reg.get<Color>(e);
    CHECK(c.r == 255);
    CHECK(c.g == 255);
    CHECK(c.b == 50);
}

TEST_CASE("spawn_score_popup_untimed: entity carries TagHUDPass",
          "[popup_entity][issue349]") {
    entt::registry reg;
    auto e = spawn_score_popup_untimed(reg, 0.0f, 0.0f, 100);
    CHECK(reg.all_of<TagHUDPass>(e));
}

TEST_CASE("spawn_score_popup_perfect: PopupDisplay initialized at spawn",
          "[popup_entity][issue349]") {
    entt::registry reg;
    auto e = spawn_score_popup_perfect(reg, 0.0f, 0.0f, 0);

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

    auto e = spawn_score_popup_perfect(reg, 100.0f, 500.0f, 200);
    REQUIRE(reg.all_of<Vector2>(e));
    REQUIRE(reg.get<Vector2>(e).y == -80.0f);

    popup_display_system(reg, 0.016f);

    const auto& vel = reg.get<Vector2>(e);
    CHECK(vel.x == 0.0f);
    CHECK(vel.y == 0.0f);

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
    create_settings_entity(reg);

    auto e = spawn_score_popup_good(reg, 0.0f, 0.0f, 100);
    popup_display_system(reg, 0.016f);

    const auto& vel = reg.get<Vector2>(e);
    CHECK(vel.y == -80.0f);
}

// #1089 / #1628 — PopupExpiredTag row count drains to zero each frame; the row
// table absorbs an arbitrary burst (no fixed reserved buffer to overflow).
TEST_CASE("popup_display_system: dense expiry burst drains expired tag rows",
          "[popup_display][issue1089]") {
    entt::registry reg;
    runtime_system_scratch_init(reg);
    constexpr int dense_count = 6;
    runtime_system_scratch_reserve(reg, dense_count);

    for (int i = 0; i < dense_count; ++i) {
        // Tiny remaining lifetime so the next tick expires every popup.
        make_untimed_popup(reg, 100, 0.001f, 1.0f);
    }

    popup_display_system(reg, 0.016f);

    // After the drain pass every PopupExpiredTag row (and its entity) is gone.
    CHECK(reg.view<PopupExpiredTag>().size() == 0);
    CHECK(reg.view<ScorePopup>().size() == 0);
}
