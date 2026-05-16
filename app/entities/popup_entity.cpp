#include "popup_entity.h"
#include "../components/scoring.h"
#include "../components/rendering.h"
#include "../components/transform.h"
#include "../constants.h"

#include <cstdio>

namespace {

// Build the popup archetype shared by every spawner. Per-tier spawners then
// emplace their tier tag and call the matching init_popup_display_*.
entt::entity create_popup_archetype(entt::registry& reg,
                                    float x, float y, int points,
                                    Color color) {
    auto popup = reg.create();
    reg.emplace<WorldPosition>(popup, WorldPosition{{x, y - 40.0f}});
    reg.emplace<Vector2>(popup, Vector2{0.0f, -80.0f});
    reg.emplace<ScorePopup>(popup, ScorePopup{points,
                                              constants::POPUP_DURATION,
                                              constants::POPUP_DURATION});
    reg.emplace<Color>(popup, color);
    reg.emplace<TagHUDPass>(popup);
    return popup;
}

void apply_base_alpha(PopupDisplay& pd, const Color& base) {
    pd.r = base.r;
    pd.g = base.g;
    pd.b = base.b;
    pd.a = base.a;
}

}  // namespace

void init_popup_display_untimed(PopupDisplay& pd,
                                const ScorePopup& sp,
                                const Color& base) {
    apply_base_alpha(pd, base);
    pd.font_size = FontSize::Small;
    std::snprintf(pd.text, sizeof(pd.text), "%d", sp.value);
}

void init_popup_display_perfect(PopupDisplay& pd, const Color& base) {
    apply_base_alpha(pd, base);
    pd.font_size = FontSize::Medium;
    std::snprintf(pd.text, sizeof(pd.text), "%s", "PERFECT");
}

void init_popup_display_good(PopupDisplay& pd, const Color& base) {
    apply_base_alpha(pd, base);
    pd.font_size = FontSize::Medium;
    std::snprintf(pd.text, sizeof(pd.text), "%s", "GOOD");
}

void init_popup_display_ok(PopupDisplay& pd, const Color& base) {
    apply_base_alpha(pd, base);
    pd.font_size = FontSize::Medium;
    std::snprintf(pd.text, sizeof(pd.text), "%s", "OK");
}

void init_popup_display_bad(PopupDisplay& pd, const Color& base) {
    apply_base_alpha(pd, base);
    pd.font_size = FontSize::Medium;
    std::snprintf(pd.text, sizeof(pd.text), "%s", "BAD");
}

entt::entity spawn_score_popup_perfect(entt::registry& reg, float x, float y, int points) {
    const Color color{80, 255, 220, 255};  // bright cyan
    auto popup = create_popup_archetype(reg, x, y, points, color);
    reg.emplace<TimingPerfectTag>(popup);
    PopupDisplay pd{};
    init_popup_display_perfect(pd, color);
    reg.emplace<PopupDisplay>(popup, pd);
    return popup;
}

entt::entity spawn_score_popup_good(entt::registry& reg, float x, float y, int points) {
    const Color color{140, 255, 80, 255};  // lime green
    auto popup = create_popup_archetype(reg, x, y, points, color);
    reg.emplace<TimingGoodTag>(popup);
    PopupDisplay pd{};
    init_popup_display_good(pd, color);
    reg.emplace<PopupDisplay>(popup, pd);
    return popup;
}

entt::entity spawn_score_popup_ok(entt::registry& reg, float x, float y, int points) {
    const Color color{255, 200, 60, 255};  // amber
    auto popup = create_popup_archetype(reg, x, y, points, color);
    reg.emplace<TimingOkTag>(popup);
    PopupDisplay pd{};
    init_popup_display_ok(pd, color);
    reg.emplace<PopupDisplay>(popup, pd);
    return popup;
}

entt::entity spawn_score_popup_bad(entt::registry& reg, float x, float y, int points) {
    const Color color{255, 90, 70, 255};  // red-orange
    auto popup = create_popup_archetype(reg, x, y, points, color);
    reg.emplace<TimingBadTag>(popup);
    PopupDisplay pd{};
    init_popup_display_bad(pd, color);
    reg.emplace<PopupDisplay>(popup, pd);
    return popup;
}

entt::entity spawn_score_popup_untimed(entt::registry& reg, float x, float y, int points) {
    const Color color{255, 255, 50, 255};  // default yellow-white
    auto popup = create_popup_archetype(reg, x, y, points, color);
    PopupDisplay pd{};
    init_popup_display_untimed(pd, reg.get<ScorePopup>(popup), color);
    reg.emplace<PopupDisplay>(popup, pd);
    return popup;
}
