// Tests for level-select dynamic spawn system (issues #1296, #1333).
//
// `level_select_dynamic_spawn_system` paints the per-level cards and
// per-(level, difficulty) buttons that the legacy controller drew
// imperatively. The system runs every time the player enters Level
// Select; a regression in card/button geometry or `ActionId` assignment
// would silently break level selection.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "test_helpers.h"
#include "components/actions.h"
#include "components/ui.h"
#include "systems/level_select_dynamic_spawn_system.h"
#include "systems/generated/screen_spawners.h"
#include "tags/tags.h"
#include "util/level_content_config.h"

namespace {

constexpr float kCardX        = 110.0f;
constexpr float kCardW        = 500.0f;
constexpr float kCardH        = 200.0f;
constexpr float kCardStartY   = 200.0f;
constexpr float kCardGap      = 40.0f;
constexpr float kDiffBtnW     = 120.0f;
constexpr float kDiffBtnH     = 50.0f;
constexpr float kDiffBtnGap   = 30.0f;
constexpr float kDiffStartX   = kCardX + 50.0f;
constexpr float kDiffYOffset  = 120.0f;

constexpr float kCardYTolerance = 0.01f;

bool near(float a, float b) {
    const float d = a - b;
    return d >= -kCardYTolerance && d <= kCardYTolerance;
}

}  // namespace

TEST_CASE("level_select_dynamic_spawn: spawns one card per level with monotonically increasing Y",
          "[level_select][ui][issue1296][issue1333]") {
    auto reg = make_registry();
    spawn_level_select_screen_full(reg);

    auto card_view = reg.view<LevelCardTag, LevelSelectScreenTag,
                              LevelIndex, UiPosition, UiBounds>();

    int card_count = 0;
    bool seen[content_config::LEVEL_COUNT] = {false, false, false};
    float y_by_index[content_config::LEVEL_COUNT] = {0.0f, 0.0f, 0.0f};

    for (auto entity : card_view) {
        ++card_count;
        const auto& idx    = card_view.get<LevelIndex>(entity);
        const auto& pos    = card_view.get<UiPosition>(entity);
        const auto& bounds = card_view.get<UiBounds>(entity);

        REQUIRE(idx.value >= 0);
        REQUIRE(idx.value < content_config::LEVEL_COUNT);
        CHECK_FALSE(seen[idx.value]);
        seen[idx.value] = true;
        y_by_index[idx.value] = pos.y;

        CHECK(near(pos.x, kCardX));
        const float expected_y = kCardStartY +
            static_cast<float>(idx.value) * (kCardH + kCardGap);
        CHECK(near(pos.y, expected_y));
        CHECK(near(bounds.w, kCardW));
        CHECK(near(bounds.h, kCardH));
    }

    CHECK(card_count == content_config::LEVEL_COUNT);

    // Monotonic Y by level_index (independent of EnTT iteration order).
    for (int i = 1; i < content_config::LEVEL_COUNT; ++i) {
        CHECK(y_by_index[i] > y_by_index[i - 1]);
    }

    // Cards are NOT generic UI buttons (issue #1296 — Level Select Pass C
    // dispatches them via `LevelIndex` reads in `ui_update_system`).
    int card_ui_button_count = 0;
    for (auto entity : reg.view<LevelCardTag, UiButtonTag>()) {
        (void)entity;
        ++card_ui_button_count;
    }
    CHECK(card_ui_button_count == 0);
}

TEST_CASE("level_select_dynamic_spawn: spawns 3 difficulty buttons per card with matching ActionId",
          "[level_select][ui][issue1296][issue1333]") {
    auto reg = make_registry();
    spawn_level_select_screen_full(reg);

    auto diff_view = reg.view<DifficultyButtonTag, UiButtonTag,
                              LevelSelectScreenTag, LevelIndex,
                              DifficultyIndex, UiPosition, UiBounds, OnPress,
                              UiLabel>();

    constexpr int kExpectedTotal =
        content_config::LEVEL_COUNT * content_config::DIFFICULTY_COUNT;

    constexpr ActionId kExpectedAction[content_config::DIFFICULTY_COUNT] = {
        ActionId::DifficultyEasy,
        ActionId::DifficultyMedium,
        ActionId::DifficultyHard,
    };

    int total = 0;
    bool seen[content_config::LEVEL_COUNT][content_config::DIFFICULTY_COUNT] = {};

    for (auto entity : diff_view) {
        ++total;
        const auto& li      = diff_view.get<LevelIndex>(entity);
        const auto& di      = diff_view.get<DifficultyIndex>(entity);
        const auto& pos     = diff_view.get<UiPosition>(entity);
        const auto& bounds  = diff_view.get<UiBounds>(entity);
        const auto& press   = diff_view.get<OnPress>(entity);
        const auto& label   = diff_view.get<UiLabel>(entity);

        REQUIRE(li.value >= 0);
        REQUIRE(li.value < content_config::LEVEL_COUNT);
        REQUIRE(di.value >= 0);
        REQUIRE(di.value < content_config::DIFFICULTY_COUNT);
        CHECK_FALSE(seen[li.value][di.value]);
        seen[li.value][di.value] = true;

        const float expected_x = kDiffStartX +
            static_cast<float>(di.value) * (kDiffBtnW + kDiffBtnGap);
        const float card_y = kCardStartY +
            static_cast<float>(li.value) * (kCardH + kCardGap);
        const float expected_y = card_y + kDiffYOffset;
        CHECK(near(pos.x, expected_x));
        CHECK(near(pos.y, expected_y));
        CHECK(near(bounds.w, kDiffBtnW));
        CHECK(near(bounds.h, kDiffBtnH));

        CHECK(press.action == kExpectedAction[di.value]);

        const std::string expected_label{content_config::DIFFICULTY_NAMES[di.value]};
        CHECK(std::string(label.text.data()) == expected_label);
    }

    CHECK(total == kExpectedTotal);

    for (int li = 0; li < content_config::LEVEL_COUNT; ++li) {
        for (int di = 0; di < content_config::DIFFICULTY_COUNT; ++di) {
            CHECK(seen[li][di]);
        }
    }
}

TEST_CASE("level_select_dynamic_spawn: despawn_level_select_screen removes every spawned entity",
          "[level_select][ui][issue1296][issue1333]") {
    auto reg = make_registry();

    CHECK(reg.view<LevelSelectScreenTag>().size() == 0);

    spawn_level_select_screen_full(reg);
    const std::size_t spawned = reg.view<LevelSelectScreenTag>().size();
    CHECK(spawned > 0);

    despawn_level_select_screen(reg);

    CHECK(reg.view<LevelSelectScreenTag>().size() == 0);
    CHECK(reg.view<LevelCardTag>().size() == 0);
    CHECK(reg.view<DifficultyButtonTag>().size() == 0);
}

TEST_CASE("level_select_dynamic_spawn: respawning idempotently rebuilds the full set",
          "[level_select][ui][issue1296][issue1333]") {
    auto reg = make_registry();
    spawn_level_select_screen_full(reg);
    const auto first_count = reg.view<LevelSelectScreenTag>().size();

    despawn_level_select_screen(reg);
    CHECK(reg.view<LevelSelectScreenTag>().size() == 0);

    spawn_level_select_screen_full(reg);
    CHECK(reg.view<LevelSelectScreenTag>().size() == first_count);
    CHECK(reg.view<LevelCardTag>().size()
          == static_cast<std::size_t>(content_config::LEVEL_COUNT));
    CHECK(reg.view<DifficultyButtonTag>().size()
          == static_cast<std::size_t>(
              content_config::LEVEL_COUNT * content_config::DIFFICULTY_COUNT));
}
