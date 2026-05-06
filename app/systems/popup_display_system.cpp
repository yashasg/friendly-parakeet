#include "../components/scoring.h"
#include <entt/entt.hpp>
#include <algorithm>
#include <vector>

// Per-frame: only fade the alpha channel. Text, font size, and base RGB are
// set once at spawn via init_popup_display() (#251). The system used to
// re-snprintf and emplace_or_replace<PopupDisplay> every tick; that re-did
// static work on every frame for every popup and forced storage churn.
void popup_display_system(entt::registry& reg, float dt) {
    std::vector<entt::entity> expired;
    auto view = reg.view<ScorePopup>();
    expired.reserve(view.size());

    for (auto [entity, popup] : view.each()) {
        popup.remaining -= dt;
        if (popup.remaining <= 0.0f) {
            expired.push_back(entity);
        }
    }

    auto display_view = reg.view<ScorePopup, PopupDisplay>();
    for (auto [entity, popup, display] : display_view.each()) {
        (void)entity;
        const float alpha_ratio = (popup.max_time > 0.0f)
            ? std::clamp(popup.remaining / popup.max_time, 0.0f, 1.0f)
            : 0.0f;
        display.a = static_cast<uint8_t>(static_cast<float>(display.base_a) * alpha_ratio);
    }

    for (auto entity : expired) {
        reg.destroy(entity);
    }
}
