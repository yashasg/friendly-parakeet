/*******************************************************************************************
*
*   Tutorial Layout - Embeddable raygui Layout (NO main, NO RAYGUI_IMPLEMENTATION)
*
*   Source: content/ui/screens/tutorial.rgl
*   Template: tools/rguilayout/templates/embeddable_layout.h
*
*   USAGE:
*      1. Include raygui.h BEFORE this header
*      2. Call TutorialLayout_Init() once to get initial state
*      3. Call TutorialLayout_Render() every frame
*      4. Check state.ContinueButtonPressed after render
*
********************************************************************************************/

#ifndef TUTORIAL_LAYOUT_H
#define TUTORIAL_LAYOUT_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TUTORIAL_LAYOUT_WIDTH  720
#define TUTORIAL_LAYOUT_HEIGHT 1280

typedef struct TutorialLayoutState {
    Vector2 Anchor01;
    bool ContinueButtonPressed;
} TutorialLayoutState;

static inline TutorialLayoutState TutorialLayout_Init(void) {
    TutorialLayoutState state = {};
    state.Anchor01 = (Vector2){ 0, 0 };
    state.ContinueButtonPressed = false;
    return state;
}

static inline void TutorialLayout_Render(TutorialLayoutState *state) {
    if (!state) return;
    GuiLabel((Rectangle){ state->Anchor01.x + 160, state->Anchor01.y + 205, 400, 48 }, "QUICK START");
    GuiLabel((Rectangle){ state->Anchor01.x + 160, state->Anchor01.y + 333, 400, 40 }, "1. MATCH THE SHAPE");
    GuiLabel((Rectangle){ state->Anchor01.x + 110, state->Anchor01.y + 538, 500, 32 }, "BECOME THE SHAPE BEFORE IT HITS YOU");
    GuiLabel((Rectangle){ state->Anchor01.x + 160, state->Anchor01.y + 640, 400, 40 }, "2. DODGE LANES");
#ifdef PLATFORM_HAS_KEYBOARD
    GuiLabel((Rectangle){ state->Anchor01.x + 110, state->Anchor01.y + 710, 500, 32 }, "USE LEFT / RIGHT ARROW KEYS");
#else
    GuiLabel((Rectangle){ state->Anchor01.x + 110, state->Anchor01.y + 710, 500, 32 }, "SWIPE LEFT OR RIGHT");
#endif
    GuiLabel((Rectangle){ state->Anchor01.x + 160, state->Anchor01.y + 806, 400, 40 }, "3. KEEP YOUR ENERGY");
    GuiLabel((Rectangle){ state->Anchor01.x + 110, state->Anchor01.y + 877, 500, 32 }, "MISSES DRAIN THE BAR ON THE LEFT");
    GuiLabel((Rectangle){ state->Anchor01.x + 110, state->Anchor01.y + 973, 500, 32 }, "CLEAR A SONG TO SET A HIGH SCORE");
    state->ContinueButtonPressed = GuiButton((Rectangle){ state->Anchor01.x + 180, state->Anchor01.y + 1075, 360, 102 }, "START");
}

#ifdef __cplusplus
}
#endif

#endif // TUTORIAL_LAYOUT_H
