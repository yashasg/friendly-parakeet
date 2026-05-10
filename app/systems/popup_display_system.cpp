#include "all_systems.h"
#include "../components/scoring.h"
#include "../components/rendering.h"
#include <vector>

namespace {

struct PopupDisplayScratch {
    std::vector<entt::entity> expired;
};

PopupDisplayScratch& popup_scratch_for(entt::registry& reg) {
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
    auto& expired = popup_scratch_for(reg).expired;
    expired.clear();

    auto fade_view = reg.view<ScorePopup, PopupDisplay, TintColor>();
    for (auto [entity, popup, pd, col] : fade_view.each()) {
        popup.remaining -= dt;
        float alpha_ratio = (popup.max_time > 0.0f)
                                ? (popup.remaining / popup.max_time)
                                : 0.0f;
        if (alpha_ratio < 0.0f) alpha_ratio = 0.0f;
        if (alpha_ratio > 1.0f) alpha_ratio = 1.0f;
        pd.a = static_cast<uint8_t>(static_cast<float>(col.a) * alpha_ratio);
        if (popup.remaining <= 0.0f) {
            expired.push_back(entity);
        }
    }

    auto expire_only_view = reg.view<ScorePopup>(entt::exclude<PopupDisplay, TintColor>);
    for (auto [entity, popup] : expire_only_view.each()) {
        popup.remaining -= dt;
        if (popup.remaining <= 0.0f) {
            expired.push_back(entity);
        }
    }

    auto popup_only_view = reg.view<ScorePopup, PopupDisplay>(entt::exclude<TintColor>);
    for (auto [entity, popup, pd] : popup_only_view.each()) {
        (void)pd;
        popup.remaining -= dt;
        if (popup.remaining <= 0.0f) {
            expired.push_back(entity);
        }
    }

    auto color_only_view = reg.view<ScorePopup, TintColor>(entt::exclude<PopupDisplay>);
    for (auto [entity, popup, col] : color_only_view.each()) {
        (void)col;
        popup.remaining -= dt;
        if (popup.remaining <= 0.0f) {
            expired.push_back(entity);
        }
    }

    for (auto entity : expired) {
        reg.destroy(entity);
    }
}
