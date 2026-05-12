// Level select layout generated from content/ui/screens/level_select.rgl.
// Dynamic card and difficulty rendering remain in controller code.

#ifndef LEVEL_SELECT_SCREEN_LAYOUT_H
#define LEVEL_SELECT_SCREEN_LAYOUT_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LEVEL_SELECT_SCREEN_LAYOUT_WIDTH  720
#define LEVEL_SELECT_SCREEN_LAYOUT_HEIGHT 1280

typedef struct LevelSelectScreenLayoutState {
    Vector2 Anchor01;
    bool StartButtonPressed;
} LevelSelectScreenLayoutState;

static inline LevelSelectScreenLayoutState LevelSelectScreenLayout_Init(void) {
    LevelSelectScreenLayoutState state = {};
    state.Anchor01 = Vector2{ 0, 0 };
    state.StartButtonPressed = false;
    return state;
}

static inline void LevelSelectScreenLayout_Render(LevelSelectScreenLayoutState *state) {
    if (!state) return;
    GuiLabel(Rectangle{ state->Anchor01.x + 210, state->Anchor01.y + 80, 300, 48 }, "SELECT LEVEL");
    state->StartButtonPressed = GuiButton(Rectangle{ state->Anchor01.x + 210, state->Anchor01.y + 1050, 300, 60 }, "START");
}

#ifdef __cplusplus
}
#endif

#endif // LEVEL_SELECT_SCREEN_LAYOUT_H
