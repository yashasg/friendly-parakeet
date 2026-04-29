/*******************************************************************************************
*
*   Paused Layout - Embeddable raygui Layout (NO main, NO RAYGUI_IMPLEMENTATION)
*
*   Source: content/ui/screens/paused.rgl
*   Template: tools/rguilayout/templates/embeddable_layout.h
*
*   USAGE:
*      1. Include raygui.h BEFORE this header
*      2. Call PausedLayout_Init() once to get initial state
*      3. Call PausedLayout_Render() every frame
*      4. Check state.ResumeButtonPressed, state.MenuButtonPressed after render
*
********************************************************************************************/

#ifndef PAUSED_LAYOUT_H
#define PAUSED_LAYOUT_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAUSED_LAYOUT_WIDTH  720
#define PAUSED_LAYOUT_HEIGHT 1280

typedef struct PausedLayoutState {
    Vector2 Anchor01;
    bool ResumeButtonPressed;
    bool MenuButtonPressed;
} PausedLayoutState;

static inline PausedLayoutState PausedLayout_Init(void) {
    PausedLayoutState state = {};
    state.Anchor01 = (Vector2){ 0, 0 };
    state.ResumeButtonPressed = false;
    state.MenuButtonPressed   = false;
    return state;
}

static inline void PausedLayout_DrawCenteredLabel(Rectangle bounds, const char *text, int text_size) {
    const int saved_text_size = GuiGetStyle(DEFAULT, TEXT_SIZE);
    const int saved_label_alignment = GuiGetStyle(LABEL, TEXT_ALIGNMENT);

    GuiSetStyle(DEFAULT, TEXT_SIZE, text_size);
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
    GuiLabel(bounds, text);

    GuiSetStyle(LABEL, TEXT_ALIGNMENT, saved_label_alignment);
    GuiSetStyle(DEFAULT, TEXT_SIZE, saved_text_size);
}

static inline void PausedLayout_Render(PausedLayoutState *state) {
    if (!state) return;
    PausedLayout_DrawCenteredLabel((Rectangle){ state->Anchor01.x + 90, state->Anchor01.y + 420, 540, 80 }, "PAUSED", 56);
    PausedLayout_DrawCenteredLabel((Rectangle){ state->Anchor01.x + 90, state->Anchor01.y + 540, 540, 36 }, "TAP RESUME TO CONTINUE", 24);
    state->ResumeButtonPressed = GuiButton((Rectangle){ state->Anchor01.x + 160, state->Anchor01.y + 620, 400, 100 }, "RESUME");
    PausedLayout_DrawCenteredLabel((Rectangle){ state->Anchor01.x + 90, state->Anchor01.y + 760, 540, 36 }, "OR RETURN TO MAIN MENU", 24);
    state->MenuButtonPressed   = GuiButton((Rectangle){ state->Anchor01.x + 160, state->Anchor01.y + 820, 400, 100 }, "MAIN MENU");
}

#ifdef __cplusplus
}
#endif

#endif // PAUSED_LAYOUT_H
