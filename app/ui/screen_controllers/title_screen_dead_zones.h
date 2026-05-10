// Title screen pointer-release dead-zones — single source of truth shared
// between title_screen_controller and the regression tests.
//
// History:
//   • #466 — controller used to duplicate the EXIT/Settings literals; that
//     drifted from the .rgl source. Resolved by deriving from the bounds
//     accessors in the generated title_layout.h.
//   • #511 — the EXIT-bounds dead-zone was wrapped in #ifndef PLATFORM_WEB,
//     so on Web a tap inside the EXIT region fell through to "tap-anywhere
//     → LevelSelect". The button itself is now hidden on Web (see
//     title_layout.h), but we keep the EXIT region as a dead-zone on
//     EVERY platform so any cached/legacy/off-by-one tap there cannot
//     silently start the game.

#ifndef SHAPESHIFTER_TITLE_SCREEN_DEAD_ZONES_H
#define SHAPESHIFTER_TITLE_SCREEN_DEAD_ZONES_H

#include <raylib.h>
#include "../generated/title_layout.h"

inline bool title_tap_in_dead_zone(Vector2 pt, const TitleLayoutState& state) {
    if (CheckCollisionPointRec(pt, TitleLayout_SettingsButtonBounds(&state))) return true;
    if (CheckCollisionPointRec(pt, TitleLayout_ExitButtonBounds(&state)))     return true;
    return false;
}

#endif // SHAPESHIFTER_TITLE_SCREEN_DEAD_ZONES_H
