/*******************************************************************************************
*
*   Song Complete Layout - Embeddable raygui Layout (NO main, NO RAYGUI_IMPLEMENTATION)
*
*   Source: content/ui/screens/song_complete.rgl
*   Template: tools/rguilayout/templates/embeddable_layout.h
*
*   USAGE:
*      1. Include raygui.h BEFORE this header
*      2. Call SongCompleteLayout_Init() once to get initial state
*      3. Call SongCompleteLayout_Render() every frame
*      4. Check RestartButtonPressed, LevelSelectButtonPressed, MenuButtonPressed
*
********************************************************************************************/

#ifndef SONG_COMPLETE_LAYOUT_H
#define SONG_COMPLETE_LAYOUT_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SONG_COMPLETE_LAYOUT_WIDTH  720
#define SONG_COMPLETE_LAYOUT_HEIGHT 1280

typedef struct SongCompleteLayoutState {
    Vector2 Anchor01;
    bool RestartButtonPressed;
    bool LevelSelectButtonPressed;
    bool MenuButtonPressed;
} SongCompleteLayoutState;

static inline SongCompleteLayoutState SongCompleteLayout_Init(void) {
    SongCompleteLayoutState state = {};
    state.Anchor01 = (Vector2){ 0, 0 };
    state.RestartButtonPressed      = false;
    state.LevelSelectButtonPressed  = false;
    state.MenuButtonPressed         = false;
    return state;
}

static inline void SongCompleteLayout_Render(SongCompleteLayoutState *state) {
    if (!state) return;
    GuiLabel((Rectangle){ state->Anchor01.x + 160, state->Anchor01.y + 340, 400, 60 }, "SONG COMPLETE");
    GuiLabel((Rectangle){ state->Anchor01.x + 260, state->Anchor01.y + 420, 200, 24 }, "SCORE");
    GuiLabel((Rectangle){ state->Anchor01.x + 210, state->Anchor01.y + 510, 300, 24 }, "HIGH SCORE");
    state->RestartButtonPressed     = GuiButton((Rectangle){ state->Anchor01.x + 220, state->Anchor01.y + 870, 280, 50 }, "RESTART");
    state->LevelSelectButtonPressed = GuiButton((Rectangle){ state->Anchor01.x + 220, state->Anchor01.y + 935, 280, 50 }, "LEVEL SELECT");
    state->MenuButtonPressed        = GuiButton((Rectangle){ state->Anchor01.x + 220, state->Anchor01.y + 1000, 280, 50 }, "MAIN MENU");
}

#ifdef __cplusplus
}
#endif

#endif // SONG_COMPLETE_LAYOUT_H
