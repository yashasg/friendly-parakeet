#include "popup_entity.h"
#include "../components/scoring.h"
#include "../components/rendering.h"
#include "../components/transform.h"
#include "../constants.h"

#include <cstdio>

void init_popup_display(PopupDisplay& pd,
                        const ScorePopup& sp,
                        const Color& base) {
    pd.r = base.r;
    pd.g = base.g;
    pd.b = base.b;
    pd.a = base.a;

    if (sp.has_timing_tier) {
        const char* grade = "BAD";
        pd.font_size = FontSize::Medium;
        switch (sp.timing_tier) {
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

    reg.emplace<WorldTransform>(popup, WorldTransform{{params.x, params.y - 40.0f}});
    reg.emplace<Vector2>(popup, Vector2{0.0f, -80.0f});
    const bool has_timing_tier = params.timing_tier.has_value();
    const TimingTier timing_tier = has_timing_tier ? *params.timing_tier : TimingTier::Ok;
    reg.emplace<ScorePopup>(popup, params.points, has_timing_tier, timing_tier,
                            constants::POPUP_DURATION, constants::POPUP_DURATION);

    // Color by timing tier (no timing → default yellow-white)
    // Issue #386: differentiate timing tiers visually for accessibility/juice.
    uint8_t pr = 255, pg = 255, pb = 50;
    if (has_timing_tier) {
        switch (timing_tier) {
            case TimingTier::Perfect: pr =  80; pg = 255; pb = 220; break; // bright cyan
            case TimingTier::Good:    pr = 140; pg = 255; pb =  80; break; // lime green
            case TimingTier::Ok:      pr = 255; pg = 200; pb =  60; break; // amber
            case TimingTier::Bad:     pr = 255; pg =  90; pb =  70; break; // red-orange
        }
    }
    Color color{pr, pg, pb, 255};
    reg.emplace<Color>(popup, color);
    reg.emplace<DrawLayer>(popup, Layer::Effects);
    reg.emplace<TagHUDPass>(popup);

    PopupDisplay pd{};
    init_popup_display(pd, reg.get<ScorePopup>(popup), color);
    reg.emplace<PopupDisplay>(popup, pd);

    return popup;
}
