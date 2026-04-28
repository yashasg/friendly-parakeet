#include "popup_entity.h"
#include "../components/scoring.h"
#include "../components/rendering.h"

#include <cstdio>

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
