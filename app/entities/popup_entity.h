#pragma once

#include <optional>
#include <entt/entt.hpp>
#include "../components/rhythm.h"  // TimingTier

struct TintColor;
struct PopupDisplay;
struct ScorePopup;

// One-time initializer for a PopupDisplay (text + font size + base color).
// Called at spawn so popup_display_system never re-formats static fields per
// frame.
void init_popup_display(PopupDisplay& pd,
                        const ScorePopup& sp,
                        const TintColor& base);

// Parameters for spawning a score popup entity.
struct PopupSpawnParams {
    float x;
    float y;
    int   points;
    std::optional<TimingTier> timing_tier;
};

// Creates a fully-initialized score popup entity:
//   WorldTransform at {x, y-40}, MotionVelocity {0, -80}, ScorePopup,
//   TintColor (by timing tier), DrawLayer::Effects, TagHUDPass, PopupDisplay.
// Audio push is the caller's responsibility.
entt::entity spawn_score_popup(entt::registry& reg, const PopupSpawnParams& params);
