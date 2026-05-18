#include "all_systems.h"
#include "popup_display_system.h"
#include "../components/scoring.h"
#include "../components/rendering.h"
#include "../components/transform.h"
#include "../entities/settings.h"

// Per-frame: only fade the alpha channel. Text, font size, and base RGB are
// set once at spawn via init_popup_display() (#251). The system used to
// re-snprintf and emplace_or_replace<PopupDisplay> every tick; that re-did
// static work on every frame for every popup and forced storage churn.
void popup_display_system(entt::registry& reg, float dt) {
    auto& scratch = reg.ctx().get<PopupDisplayScratch>();
    auto& expired = scratch.expired;
    expired.clear();

    // Reduce-motion (#534): zero the kinetic envelope of decorative
    // popup drift while leaving text/colour/value (informational
    // channel) untouched. The alpha-fade still runs so popups expire
    // on schedule.
    const auto* settings_ptr = find_settings_state(reg);
    const bool reduce_motion = settings_ptr && settings_ptr->reduce_motion;
    if (reduce_motion) {
        auto drift_view = reg.view<ScorePopup, Vector2>();
        for (auto [entity, popup, vel] : drift_view.each()) {
            (void)entity; (void)popup;
            vel.x = 0.0f;
            vel.y = 0.0f;
        }
    }

    auto fade_view = reg.view<ScorePopup, PopupDisplay, Color>();
    for (auto [entity, popup, pd, col] : fade_view.each()) {
        popup.remaining -= dt;
        float alpha_ratio = (popup.max_time > 0.0f)
                                ? (popup.remaining / popup.max_time)
                                : 0.0f;
        if (alpha_ratio < 0.0f) alpha_ratio = 0.0f;
        if (alpha_ratio > 1.0f) alpha_ratio = 1.0f;
        pd.a = static_cast<uint8_t>(static_cast<float>(col.a) * alpha_ratio);
        if (popup.remaining <= 0.0f) {
            if (expired.size() >= expired.capacity()) {
                ++scratch.capacity_exceeded_count;
            }
            expired.push_back(entity);
        }
    }

    for (auto entity : expired) {
        reg.destroy(entity);
    }
}
