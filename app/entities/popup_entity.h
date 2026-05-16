#pragma once

#include <entt/entt.hpp>
#include <raylib.h>
#include "../components/rhythm.h"  // TimingGrade
#include "../tags/tags.h"          // TimingPerfectTag etc.

struct PopupDisplay;
struct ScorePopup;

// One-time initializer for an untimed PopupDisplay (numeric value, Small font).
// Called at spawn so popup_display_system never re-formats static fields per
// frame. Per-tier graded popups use the per-tier init functions below.
void init_popup_display_untimed(PopupDisplay& pd,
                                const ScorePopup& sp,
                                const Color& base);

// Per-tier popup-display initializers. Each bakes a constant text label and
// FontSize::Medium into the PopupDisplay alongside the supplied base color.
// Per #1202/#1204: these replace the former switch on a TimingTier
// discriminator with one initializer per former enum value.
void init_popup_display_perfect(PopupDisplay& pd, const Color& base);
void init_popup_display_good   (PopupDisplay& pd, const Color& base);
void init_popup_display_ok     (PopupDisplay& pd, const Color& base);
void init_popup_display_bad    (PopupDisplay& pd, const Color& base);

// Per-tier popup spawn functions. Each emplaces the full popup archetype
// (WorldPosition, Vector2 drift, ScorePopup, Color, TagHUDPass, PopupDisplay)
// plus the matching per-tier tag on the popup entity so renderers can identify
// the popup's tier via tag presence (no enum lookup).
//
// Per #1202/#1204: replaces the prior `spawn_score_popup` that switched on a
// TimingTier discriminator for both color and text — those constants are now
// inlined per-spawner.
entt::entity spawn_score_popup_perfect(entt::registry& reg, float x, float y, int points);
entt::entity spawn_score_popup_good   (entt::registry& reg, float x, float y, int points);
entt::entity spawn_score_popup_ok     (entt::registry& reg, float x, float y, int points);
entt::entity spawn_score_popup_bad    (entt::registry& reg, float x, float y, int points);
entt::entity spawn_score_popup_untimed(entt::registry& reg, float x, float y, int points);
