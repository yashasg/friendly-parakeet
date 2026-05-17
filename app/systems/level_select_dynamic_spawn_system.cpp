#include "level_select_dynamic_spawn_system.h"

#include "../components/actions.h"
#include "../components/ui.h"
#include "../tags/tags.h"
#include "generated/screen_spawners.h"
#include "../util/level_content_config.h"

// Geometry constants — single source of truth, mirrored by the matching
// renderer block in `ui_render_system.cpp`. Edits here must keep the
// renderer constants in sync (kept as separate constexpr there to avoid
// dragging this header into the render TU's already-busy include set).

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

float card_y_for(int level_index) {
    return kCardStartY + static_cast<float>(level_index) * (kCardH + kCardGap);
}

float diff_x_for(int diff_index) {
    return kDiffStartX +
           static_cast<float>(diff_index) * (kDiffBtnW + kDiffBtnGap);
}

ActionId action_for_difficulty(int diff_index) {
    // Per Fabian Principle 1: indexed table, no switch on the discriminator.
    constexpr ActionId kRow[content_config::DIFFICULTY_COUNT] = {
        ActionId::DifficultyEasy,
        ActionId::DifficultyMedium,
        ActionId::DifficultyHard,
    };
    if (diff_index < 0 || diff_index >= content_config::DIFFICULTY_COUNT) {
        return ActionId::None;
    }
    return kRow[diff_index];
}

}  // namespace

void level_select_dynamic_spawn(entt::registry& reg) {
    // Level cards — one per `content_config::LEVELS` row. Cards are NOT
    // `UiButtonTag` entities (issue #1296): a card-region click is
    // handled by the Level Select Pass C in `ui_update_system`, which
    // reads the entity's `LevelIndex` and writes it to
    // `LevelSelectState::selected_level`. Keeping cards out of the
    // generic button table avoids competing with the diff-button hit
    // pass (smaller hit area wins, matching the legacy raygui-overlay
    // behaviour where the diff-button GuiButton intercepted clicks that
    // would otherwise have hit the card background).
    for (int i = 0; i < content_config::LEVEL_COUNT; ++i) {
        const auto e = reg.create();
        reg.emplace<LevelSelectScreenTag>(e);
        reg.emplace<LevelCardTag>(e);
        reg.emplace<LevelIndex>(e, i);
        reg.emplace<UiPosition>(e, kCardX, card_y_for(i));
        reg.emplace<UiBounds>(e, kCardW, kCardH);
        auto& label = reg.emplace<UiLabel>(e);
        ui_label_set(label, content_config::LEVELS[i].title);
    }

    // Difficulty buttons — one per (level, difficulty) pair. Position is
    // baked at spawn time; the render path filters to the active level
    // via `LevelIndex == LevelSelectState::selected_level`. Each button
    // carries the matching `ActionId::Difficulty*` so the dispatch table
    // already knows what to do.
    for (int li = 0; li < content_config::LEVEL_COUNT; ++li) {
        const float diff_y = card_y_for(li) + kDiffYOffset;
        for (int di = 0; di < content_config::DIFFICULTY_COUNT; ++di) {
            const auto e = reg.create();
            reg.emplace<LevelSelectScreenTag>(e);
            reg.emplace<DifficultyButtonTag>(e);
            reg.emplace<UiButtonTag>(e);
            reg.emplace<LevelIndex>(e, li);
            reg.emplace<DifficultyIndex>(e, di);
            reg.emplace<UiPosition>(e, diff_x_for(di), diff_y);
            reg.emplace<UiBounds>(e, kDiffBtnW, kDiffBtnH);
            auto& label = reg.emplace<UiLabel>(e);
            ui_label_set(label, content_config::DIFFICULTY_NAMES[di]);
            reg.emplace<OnPress>(e, action_for_difficulty(di));
        }
    }
}

void spawn_level_select_screen_full(entt::registry& reg) {
    spawn_level_select_screen(reg);
    level_select_dynamic_spawn(reg);
}
