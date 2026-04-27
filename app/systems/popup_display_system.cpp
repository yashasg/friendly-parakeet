#include "all_systems.h"
#include "../components/scoring.h"
#include "../components/rendering.h"
#include "../components/lifetime.h"
#include <raylib.h>
#include <cstdio>
#include <cstring>

// One-time formatter for PopupDisplay's static fields (text, font size, base
// color). Called at spawn from scoring_system; popup_display_system itself
// only mutates the per-frame alpha after this (#251).
void init_popup_display(PopupDisplay& pd,
                        const ScorePopup& sp,
                        const Color& base) {
    pd.r = base.r;
    pd.g = base.g;
    pd.b = base.b;
    pd.a = base.a;

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

// Per-frame: only fade the alpha channel. Text, font size, and base RGB are
// set once at spawn via init_popup_display() (#251). The system used to
// re-snprintf and emplace_or_replace<PopupDisplay> every tick; that re-did
// static work on every frame for every popup and forced storage churn.
void popup_display_system(entt::registry& reg, float /*dt*/) {
    auto view = reg.view<PopupDisplay, Color, Lifetime>();
    for (auto [entity, pd, col, life] : view.each()) {
        float alpha_ratio = (life.max_time > 0.0f)
                                ? (life.remaining / life.max_time)
                                : 0.0f;
        if (alpha_ratio < 0.0f) alpha_ratio = 0.0f;
        if (alpha_ratio > 1.0f) alpha_ratio = 1.0f;
        pd.a = static_cast<uint8_t>(static_cast<float>(col.a) * alpha_ratio);
    }
}
