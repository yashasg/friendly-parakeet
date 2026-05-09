#include <catch2/catch_test_macros.hpp>
#include <type_traits>
#include <entt/entt.hpp>

#include "components/rendering.h"

// Render-pass tag invariants.
static_assert(std::is_empty_v<TagWorldPass>,
    "TagWorldPass must be an empty struct (tag component).");
static_assert(std::is_empty_v<TagEffectsPass>,
    "TagEffectsPass must be an empty struct (tag component).");
static_assert(std::is_empty_v<TagHUDPass>,
    "TagHUDPass must be an empty struct (tag component).");

static_assert(std::is_standard_layout_v<TagWorldPass>,
    "TagWorldPass must be standard-layout for EnTT compatibility.");
static_assert(std::is_standard_layout_v<TagEffectsPass>,
    "TagEffectsPass must be standard-layout for EnTT compatibility.");
static_assert(std::is_standard_layout_v<TagHUDPass>,
    "TagHUDPass must be standard-layout for EnTT compatibility.");

static_assert(!std::is_same_v<TagWorldPass, TagEffectsPass>,
    "Render-pass tags must be distinct types.");
static_assert(!std::is_same_v<TagWorldPass, TagHUDPass>,
    "Render-pass tags must be distinct types.");
static_assert(!std::is_same_v<TagEffectsPass, TagHUDPass>,
    "Render-pass tags must be distinct types.");

TEST_CASE("TagWorldPass: emplace and has_any_of query works (Slice 0)",
          "[render_tags][model_slice]") {
    entt::registry reg;
    auto e = reg.create();
    reg.emplace<TagWorldPass>(e);
    CHECK(reg.all_of<TagWorldPass>(e));
    CHECK_FALSE(reg.all_of<TagEffectsPass>(e));
    CHECK_FALSE(reg.all_of<TagHUDPass>(e));
}

TEST_CASE("Each render-pass tag creates a distinct EnTT component pool (Slice 0)",
          "[render_tags][model_slice]") {
    entt::registry reg;

    auto e1 = reg.create(); reg.emplace<TagWorldPass>(e1);
    auto e2 = reg.create(); reg.emplace<TagEffectsPass>(e2);
    auto e3 = reg.create(); reg.emplace<TagHUDPass>(e3);

    CHECK( reg.all_of<TagWorldPass>(e1));
    CHECK(!reg.all_of<TagEffectsPass>(e1));
    CHECK(!reg.all_of<TagHUDPass>(e1));

    CHECK(!reg.all_of<TagWorldPass>(e2));
    CHECK( reg.all_of<TagEffectsPass>(e2));
    CHECK(!reg.all_of<TagHUDPass>(e2));
}
