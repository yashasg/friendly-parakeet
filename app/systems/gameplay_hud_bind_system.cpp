#include "gameplay_hud_bind_system.h"

#include "../components/scoring.h"
#include "../components/ui.h"
#include "../tags/tags.h"

#include <array>
#include <cstdio>

namespace {

// Canonical slot positions from `content/ui/screens/gameplay.rgl`. The
// codegen bakes these exact (x, y) into `spawn_gameplay_screen`, so a
// layout edit that moves a slot will visibly miss its bind in playtest
// before any production build (matches the `game_over_scoreboard` /
// `settings_screen` bind convention).
constexpr float kScoreSlotX     =  60.0f, kScoreSlotY     = 20.0f;
constexpr float kHighScoreSlotX =  60.0f, kHighScoreSlotY = 50.0f;
constexpr float kChainSlotX     =  80.0f, kChainSlotY     = 80.0f;
constexpr float kEnergyLabelX   =  10.0f, kEnergyLabelY   = 745.0f;

// Font sizes mirror the legacy `render_gameplay_hud_screen_ui` text
// renders (the values pre-migration were `GuiSetStyle(DEFAULT, TEXT_SIZE, …)`
// switches with these literals).
constexpr int kScoreFontSize        = 28;
constexpr int kHighScoreFontSize    = 18;
constexpr int kChainFontSizeRegular = 18;
constexpr int kChainFontSizeMeaningful = 20;
constexpr int kEnergyLabelFontSize  = 16;

// Alpha cues match the legacy `GuiSetAlpha(…)` overrides.
constexpr float kHighScoreAlpha          = 0.7f;
constexpr float kChainAlphaRegular       = 0.7f;
constexpr float kChainAlphaMeaningful    = 0.95f;
constexpr float kEnergyLabelAlpha        = 0.8f;

// Chain threshold: a "meaningful" chain is the explicit `>= 5` branch in
// the legacy controller (drives the "!" suffix, the larger font, and the
// background pulse halo).
constexpr int kChainMeaningfulThreshold = 5;
// Chain visibility threshold (legacy `score->chain_count >= 2`).
constexpr int kChainVisibleThreshold = 2;

// Per-binder prefix (`GameplayHud*`) avoids an anonymous-namespace ODR
// collision with the sibling `*_bind_system.cpp` files when CMake Unity
// chunking happens to place two binders in the same jumbo TU (issue #1329).
struct GameplayHudBindContext {
    const ScoreState*           score;
    const ScoreDisplay*         display;
    const CurrentSongHighScore* current;
};

bool bind_pos_matches(float x, float y, float ex, float ey) noexcept {
    return ex == x && ey == y;
}

void bind_score(const GameplayHudBindContext& ctx, entt::registry& reg, entt::entity e, UiLabel& label) {
    if (ctx.display == nullptr) {
        ui_label_set(label, "");
    } else {
        ui_label_set_int(label, ctx.display->displayed);
    }
    reg.emplace_or_replace<UiLabelFontSize>(e, UiLabelFontSize{kScoreFontSize});
    reg.emplace_or_replace<UiLabelAlpha>(e, UiLabelAlpha{1.0f});
}

void bind_high_score(const GameplayHudBindContext& ctx, entt::registry& reg, entt::entity e, UiLabel& label) {
    const int32_t high = ctx.current ? ctx.current->value : 0;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "BEST: %d", high);
    ui_label_set(label, buf);
    reg.emplace_or_replace<UiLabelFontSize>(e, UiLabelFontSize{kHighScoreFontSize});
    reg.emplace_or_replace<UiLabelAlpha>(e, UiLabelAlpha{kHighScoreAlpha});
}

void bind_chain(const GameplayHudBindContext& ctx, entt::registry& reg, entt::entity e, UiLabel& label) {
    const int chain = ctx.score ? ctx.score->chain_count : 0;
    if (chain < kChainVisibleThreshold) {
        ui_label_set(label, "");
        if (reg.all_of<ChainBgPulseTag>(e)) reg.remove<ChainBgPulseTag>(e);
        return;
    }
    const bool meaningful = chain >= kChainMeaningfulThreshold;
    char buf[32];
    std::snprintf(buf, sizeof(buf), meaningful ? "CHAIN %d!" : "CHAIN %d", chain);
    ui_label_set(label, buf);
    reg.emplace_or_replace<UiLabelFontSize>(e, UiLabelFontSize{
        meaningful ? kChainFontSizeMeaningful : kChainFontSizeRegular});
    reg.emplace_or_replace<UiLabelAlpha>(e, UiLabelAlpha{
        meaningful ? kChainAlphaMeaningful : kChainAlphaRegular});
    if (meaningful) {
        reg.emplace_or_replace<ChainBgPulseTag>(e);
    } else if (reg.all_of<ChainBgPulseTag>(e)) {
        reg.remove<ChainBgPulseTag>(e);
    }
}

void bind_energy_label(const GameplayHudBindContext& /*ctx*/, entt::registry& reg, entt::entity e, UiLabel& /*label*/) {
    // Static "ENERGY" text is set at spawn time by the codegen; this bind
    // just keeps the per-slot font size + alpha in sync each frame.
    reg.emplace_or_replace<UiLabelFontSize>(e, UiLabelFontSize{kEnergyLabelFontSize});
    reg.emplace_or_replace<UiLabelAlpha>(e, UiLabelAlpha{kEnergyLabelAlpha});
}

using GameplayHudSlotBindFn = void (*)(const GameplayHudBindContext&, entt::registry&, entt::entity, UiLabel&);

struct GameplayHudSlotRow {
    float x;
    float y;
    GameplayHudSlotBindFn fn;
};

constexpr std::array<GameplayHudSlotRow, 4> kGameplayHudSlots = {{
    {kScoreSlotX,     kScoreSlotY,     &bind_score},
    {kHighScoreSlotX, kHighScoreSlotY, &bind_high_score},
    {kChainSlotX,     kChainSlotY,     &bind_chain},
    {kEnergyLabelX,   kEnergyLabelY,   &bind_energy_label},
}};

}  // namespace

void gameplay_hud_bind_system(entt::registry& reg) {
    auto view = reg.view<GameplayHudTag, UiLabelTag, UiPosition, UiLabel>();
    if (view.begin() == view.end()) return;

    const GameplayHudBindContext ctx{
        reg.ctx().find<ScoreState>(),
        reg.ctx().find<ScoreDisplay>(),
        reg.ctx().find<CurrentSongHighScore>(),
    };

    for (auto entity : view) {
        const auto& pos = view.get<UiPosition>(entity);
        auto& label     = view.get<UiLabel>(entity);
        for (const auto& row : kGameplayHudSlots) {
            if (bind_pos_matches(row.x, row.y, pos.x, pos.y)) {
                row.fn(ctx, reg, entity, label);
                break;
            }
        }
    }
}
