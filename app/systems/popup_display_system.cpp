#include "all_systems.h"
#include "../components/scoring.h"
#include "../components/rendering.h"
#include <vector>

namespace {

struct PopupDisplayScratch {
    std::vector<entt::entity> expired;
};

PopupDisplayScratch& scratch_for(entt::registry& reg) {
    if (auto* scratch = reg.ctx().find<PopupDisplayScratch>()) {
        return *scratch;
    }
    return reg.ctx().emplace<PopupDisplayScratch>();
}

}  // namespace

// Per-frame: only fade the alpha channel. Text, font size, and base RGB are
// set once at spawn via init_popup_display() (#251). The system used to
// re-snprintf and emplace_or_replace<PopupDisplay> every tick; that re-did
// static work on every frame for every popup and forced storage churn.
void popup_display_system(entt::registry& reg, float dt) {
    auto& expired = scratch_for(reg).expired;
    expired.clear();

    auto view = reg.view<ScorePopup>();
    for (auto [entity, popup] : view.each()) {
        popup.remaining -= dt;
        auto* pd = reg.try_get<PopupDisplay>(entity);
        auto* col = reg.try_get<Color>(entity);
        if (pd && col) {
            float alpha_ratio = (popup.max_time > 0.0f)
                                    ? (popup.remaining / popup.max_time)
                                    : 0.0f;
            if (alpha_ratio < 0.0f) alpha_ratio = 0.0f;
            if (alpha_ratio > 1.0f) alpha_ratio = 1.0f;
            pd->a = static_cast<uint8_t>(static_cast<float>(col->a) * alpha_ratio);
        }
        if (popup.remaining <= 0.0f) {
            expired.push_back(entity);
        }
    }

    for (auto entity : expired) {
        reg.destroy(entity);
    }
}
