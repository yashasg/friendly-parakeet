// Game over layout generated from content/ui/screens/game_over.rgl.
// Do not edit behavior here; regenerate from the .rgl source.

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
    state.Anchor01 = Vector2{ 0, 0 };
    state.RestartButtonPressed      = false;
    state.LevelSelectButtonPressed  = false;
    state.MenuButtonPressed         = false;
    return state;
}

static inline void GameOverLayout_DrawCenteredLabel(Rectangle bounds, const char *text, int text_size) {
    const int saved_text_size = GuiGetStyle(DEFAULT, TEXT_SIZE);
    const int saved_label_alignment = GuiGetStyle(LABEL, TEXT_ALIGNMENT);

    GuiSetStyle(DEFAULT, TEXT_SIZE, text_size);
    GuiSetStyle(LABEL, TEXT_ALIGNMENT, TEXT_ALIGN_CENTER);
    GuiLabel(bounds, text);

    GuiSetStyle(LABEL, TEXT_ALIGNMENT, saved_label_alignment);
    GuiSetStyle(DEFAULT, TEXT_SIZE, saved_text_size);
}

static inline void GameOverLayout_Render(GameOverLayoutState *state) {
    if (!state) return;
    GuiLabel(Rectangle{ state->Anchor01.x + 210, state->Anchor01.y + 440, 300, 60 }, "GAME OVER");
    GameOverLayout_DrawCenteredLabel(Rectangle{ state->Anchor01.x + 210, state->Anchor01.y + 510, 300, 30 }, "SCORE", 24);
    GameOverLayout_DrawCenteredLabel(Rectangle{ state->Anchor01.x + 210, state->Anchor01.y + 600, 300, 30 }, "HIGH SCORE", 24);
    state->RestartButtonPressed     = GuiButton(Rectangle{ state->Anchor01.x + 220, state->Anchor01.y + 870, 280, 50 }, "RESTART");
    state->LevelSelectButtonPressed = GuiButton(Rectangle{ state->Anchor01.x + 220, state->Anchor01.y + 935, 280, 50 }, "LEVEL SELECT");
    state->MenuButtonPressed        = GuiButton(Rectangle{ state->Anchor01.x + 220, state->Anchor01.y + 1000, 280, 50 }, "MAIN MENU");
}

#ifdef __cplusplus
}
#endif

#endif // GAME_OVER_LAYOUT_H
