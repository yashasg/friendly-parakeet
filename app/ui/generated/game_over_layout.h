/*******************************************************************************************
*
*   Game Over Layout - Embeddable raygui Layout (NO main, NO RAYGUI_IMPLEMENTATION)
*
*   Source: content/ui/screens/game_over.rgl
*   Template: tools/rguilayout/templates/embeddable_layout.h
*
*   USAGE:
*      1. Include raygui.h BEFORE this header
*      2. Call GameOverLayout_Init() once to get initial state
*      3. Call GameOverLayout_Render() every frame
*      4. Check RestartButtonPressed, LevelSelectButtonPressed, MenuButtonPressed
*
********************************************************************************************/

#ifndef GAME_OVER_LAYOUT_H
#define GAME_OVER_LAYOUT_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GAME_OVER_LAYOUT_WIDTH  720
#define GAME_OVER_LAYOUT_HEIGHT 1280

typedef struct GameOverLayoutState {
    Vector2 Anchor01;
    bool RestartButtonPressed;
    bool LevelSelectButtonPressed;
    bool MenuButtonPressed;
} GameOverLayoutState;

static inline GameOverLayoutState GameOverLayout_Init(void) {
    GameOverLayoutState state = {};
    state.Anchor01 = (Vector2){ 0, 0 };
    state.RestartButtonPressed      = false;
    state.LevelSelectButtonPressed  = false;
    state.MenuButtonPressed         = false;
    return state;
}

static inline void GameOverLayout_Render(GameOverLayoutState *state) {
    if (!state) return;
    GuiLabel((Rectangle){ state->Anchor01.x + 210, state->Anchor01.y + 440, 300, 60 }, "GAME OVER");
    state->RestartButtonPressed     = GuiButton((Rectangle){ state->Anchor01.x + 220, state->Anchor01.y + 870, 280, 50 }, "RESTART");
    state->LevelSelectButtonPressed = GuiButton((Rectangle){ state->Anchor01.x + 220, state->Anchor01.y + 935, 280, 50 }, "LEVEL SELECT");
    state->MenuButtonPressed        = GuiButton((Rectangle){ state->Anchor01.x + 220, state->Anchor01.y + 1000, 280, 50 }, "MAIN MENU");
}

#ifdef __cplusplus
}
#endif

#endif // GAME_OVER_LAYOUT_H
