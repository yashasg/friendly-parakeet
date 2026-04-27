#include "all_systems.h"
#include "../components/scoring.h"
#include "../components/rendering.h"
#include "../components/lifetime.h"
#include <raylib.h>
#include <cstdio>
#include <cstring>

void popup_display_system(entt::registry& reg, float /*dt*/) {
    auto view = reg.view<ScorePopup, ScreenPosition, Color, Lifetime>();
    for (auto [entity, popup, sp, col, life] : view.each()) {
        float alpha_ratio = life.remaining / life.max_time;
        Color faded = Fade(col, alpha_ratio);

        PopupDisplay pd{};
        pd.r = faded.r;
        pd.g = faded.g;
        pd.b = faded.b;
        pd.a = faded.a;

        if (popup.timing_tier.has_value()) {
            const char* grade = "BAD";
            pd.font_size = FontSize::Small;
            switch (*popup.timing_tier) {
                case TimingTier::Perfect: grade = "PERFECT"; pd.font_size = FontSize::Medium; break;
                case TimingTier::Good:    grade = "GOOD";    break;
                case TimingTier::Ok:      grade = "OK";      break;
                case TimingTier::Bad:                        break;
            }
            std::snprintf(pd.text, sizeof(pd.text), "%s", grade);
        } else {
            std::snprintf(pd.text, sizeof(pd.text), "%d", popup.value);
        }

        reg.emplace_or_replace<PopupDisplay>(entity, pd);
    }
}
