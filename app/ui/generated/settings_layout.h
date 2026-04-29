/*******************************************************************************************
*
*   Settings Layout - Embeddable raygui Layout (NO main, NO RAYGUI_IMPLEMENTATION)
*
*   Source: content/ui/screens/settings.rgl
*   Template: tools/rguilayout/templates/embeddable_layout.h
*
*   NOTE: HapticsToggle and ReduceMotionToggle have dynamic labels.
*         The screen controller renders them separately with runtime-resolved text.
*         Call SettingsLayout_RenderStatic for non-toggle controls.
*
*   USAGE:
*      1. Include raygui.h BEFORE this header
*      2. Call SettingsLayout_Init() once to get initial state
*      3. Call SettingsLayout_RenderStatic() + handle toggle buttons in screen controller
*      4. Check button result fields after render
*
********************************************************************************************/

#ifndef SETTINGS_LAYOUT_H
#define SETTINGS_LAYOUT_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SETTINGS_LAYOUT_WIDTH  720
#define SETTINGS_LAYOUT_HEIGHT 1280

typedef struct SettingsLayoutState {
    Vector2 Anchor01;
    bool AudioOffsetMinusPressed;
    bool AudioOffsetPlusPressed;
    bool HapticsTogglePressed;
    bool ReduceMotionTogglePressed;
    bool CloseButtonPressed;
} SettingsLayoutState;

static inline SettingsLayoutState SettingsLayout_Init(void) {
    SettingsLayoutState state = {};
    state.Anchor01 = (Vector2){ 0, 0 };
    state.AudioOffsetMinusPressed   = false;
    state.AudioOffsetPlusPressed    = false;
    state.HapticsTogglePressed      = false;
    state.ReduceMotionTogglePressed = false;
    state.CloseButtonPressed        = false;
    return state;
}

// Render static (non-dynamic-label) controls only.
// Adapter is responsible for rendering the two toggle buttons with live labels.
static inline void SettingsLayout_RenderStatic(SettingsLayoutState *state) {
    if (!state) return;
    GuiLabel((Rectangle){ state->Anchor01.x + 210, state->Anchor01.y + 400, 300, 48 }, "SETTINGS");
    GuiLabel((Rectangle){ state->Anchor01.x + 144, state->Anchor01.y + 510, 200, 40 }, "Audio Offset");
    state->AudioOffsetMinusPressed = GuiButton((Rectangle){ state->Anchor01.x + 180, state->Anchor01.y + 560,  72, 77 }, "-");
    state->AudioOffsetPlusPressed  = GuiButton((Rectangle){ state->Anchor01.x + 468, state->Anchor01.y + 560,  72, 77 }, "+");
    state->CloseButtonPressed      = GuiButton((Rectangle){ state->Anchor01.x + 260, state->Anchor01.y + 1040, 200, 100 }, "BACK");
}

#ifdef __cplusplus
}
#endif

#endif // SETTINGS_LAYOUT_H
