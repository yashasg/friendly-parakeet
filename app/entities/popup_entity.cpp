#include "popup_entity.h"
#include "../components/scoring.h"
#include "../components/transform.h"
#include "../components/render_tags.h"
#include "../constants.h"

#include <cstdio>
#include <optional>

namespace {

SDL_Color popup_color_for(const std::optional<TimingTier> timing_tier) {
    if (!timing_tier.has_value()) {
        return SDL_Color{255, 255, 50, 255};
    }

    switch (*timing_tier) {
        case TimingTier::Perfect: return SDL_Color{100, 255, 100, 255};  // green
        case TimingTier::Good:    return SDL_Color{180, 255, 100, 255};  // yellow-green
        case TimingTier::Ok:      return SDL_Color{255, 255, 100, 255};  // yellow
        case TimingTier::Bad:     return SDL_Color{255, 150, 100, 255};  // orange
    }

    return SDL_Color{255, 255, 50, 255};
}

}  // namespace

void init_popup_display(PopupDisplay& pd, const ScorePopup& sp, const SDL_Color& base) {
    pd.r = base.r;
    pd.g = base.g;
    pd.b = base.b;
    pd.a = base.a;
    pd.base_a = base.a;

    if (sp.timing_tier.has_value()) {
        const char* grade = "BAD";
        pd.font_size = FontSize::Small;
        switch (*sp.timing_tier) {
            case TimingTier::Perfect: grade = "PERFECT"; pd.font_size = FontSize::Medium; break;
            case TimingTier::Good:    grade = "GOOD";    break;
            case TimingTier::Ok:      grade = "OK";      break;
            case TimingTier::Bad:                        break;
        }
        std::snprintf(pd.text, sizeof(pd.text), "%s", grade);
    } else {
        pd.font_size = FontSize::Small;
        std::snprintf(pd.text, sizeof(pd.text), "%d", sp.value);
    }
}

entt::entity spawn_score_popup(entt::registry& reg, const PopupSpawnParams& params) {
    auto popup = reg.create();
    const ScorePopup score_popup{
        params.points,
        params.timing_tier,
        constants::POPUP_DURATION,
        constants::POPUP_DURATION
    };

    reg.emplace<WorldTransform>(popup, WorldTransform{{params.x, params.y - 40.0f}});
    reg.emplace<MotionVelocity>(popup, MotionVelocity{{0.0f, -80.0f}});
    reg.emplace<ScorePopup>(popup, score_popup);

    const auto color = popup_color_for(params.timing_tier);
    reg.emplace<SDL_Color>(popup, color);
    reg.emplace<DrawLayer>(popup, Layer::Effects);
    reg.emplace<TagHUDPass>(popup);

    PopupDisplay pd{};
    init_popup_display(pd, score_popup, color);
    reg.emplace<PopupDisplay>(popup, pd);

    return popup;
}
