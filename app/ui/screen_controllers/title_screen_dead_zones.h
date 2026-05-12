// Title pointer-release dead-zones shared by controller and regression tests.
// Keep EXIT covered on every platform; see #466 and #511 for history.

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
