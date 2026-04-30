/*******************************************************************************************
*
*   Gameplay HUD Layout - Embeddable raygui Layout (NO main, NO RAYGUI_IMPLEMENTATION)
*
*   Source: content/ui/screens/gameplay.rgl
*   Template: tools/rguilayout/templates/embeddable_layout.h
*
 *   NOTE: Provides Pause + gameplay shape buttons.
 *         Dynamic energy bar and approach rings remain rendered by controller code.
*
*   The name "GameplayHudLayoutState" avoids collision with "HudLayout" in
*   ui_layout_cache.h which is a separate ECS cache struct.
*
********************************************************************************************/

#ifndef GAMEPLAY_HUD_LAYOUT_H
#define GAMEPLAY_HUD_LAYOUT_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GAMEPLAY_HUD_LAYOUT_WIDTH  720
#define GAMEPLAY_HUD_LAYOUT_HEIGHT 1280

typedef struct GameplayHudLayoutState {
    Vector2 Anchor01;
    bool PauseButtonPressed;
    bool CircleButtonPressed;
    bool SquareButtonPressed;
    bool TriangleButtonPressed;
} GameplayHudLayoutState;

static inline GameplayHudLayoutState GameplayHudLayout_Init(void) {
    GameplayHudLayoutState state = {};
    state.Anchor01 = (Vector2){ 0, 0 };
    state.PauseButtonPressed = false;
    state.CircleButtonPressed = false;
    state.SquareButtonPressed = false;
    state.TriangleButtonPressed = false;
    return state;
}

static inline Rectangle GameplayHudLayout_PauseButtonBounds(const GameplayHudLayoutState *state) {
    return (Rectangle){ state->Anchor01.x + 620, state->Anchor01.y + 10, 80, 50 };
}

static inline Rectangle GameplayHudLayout_CircleButtonBounds(const GameplayHudLayoutState *state) {
    return (Rectangle){ state->Anchor01.x + 60, state->Anchor01.y + 1140, 140, 100 };
}

static inline Rectangle GameplayHudLayout_SquareButtonBounds(const GameplayHudLayoutState *state) {
    return (Rectangle){ state->Anchor01.x + 220, state->Anchor01.y + 1140, 140, 100 };
}

static inline Rectangle GameplayHudLayout_TriangleButtonBounds(const GameplayHudLayoutState *state) {
    return (Rectangle){ state->Anchor01.x + 380, state->Anchor01.y + 1140, 140, 100 };
}

static inline bool GameplayHudLayout_InputOnlyButton(Rectangle bounds) {
    static const int kHiddenProps[] = {
        BORDER_COLOR_NORMAL, BASE_COLOR_NORMAL, TEXT_COLOR_NORMAL,
        BORDER_COLOR_FOCUSED, BASE_COLOR_FOCUSED, TEXT_COLOR_FOCUSED,
        BORDER_COLOR_PRESSED, BASE_COLOR_PRESSED, TEXT_COLOR_PRESSED,
        BORDER_COLOR_DISABLED, BASE_COLOR_DISABLED, TEXT_COLOR_DISABLED
    };
    int saved_props[sizeof(kHiddenProps) / sizeof(kHiddenProps[0])] = {0};
    const int transparent = ColorToInt(BLANK);

    for (int i = 0; i < (int)(sizeof(kHiddenProps) / sizeof(kHiddenProps[0])); ++i) {
        saved_props[i] = GuiGetStyle(BUTTON, kHiddenProps[i]);
        GuiSetStyle(BUTTON, kHiddenProps[i], transparent);
    }

    bool pressed = GuiButton(bounds, "");

    for (int i = 0; i < (int)(sizeof(kHiddenProps) / sizeof(kHiddenProps[0])); ++i) {
        GuiSetStyle(BUTTON, kHiddenProps[i], saved_props[i]);
    }

    return pressed;
}

static inline void GameplayHudLayout_Render(GameplayHudLayoutState *state) {
    if (!state) return;
    state->PauseButtonPressed = GuiButton(GameplayHudLayout_PauseButtonBounds(state), "||");
    state->CircleButtonPressed = GameplayHudLayout_InputOnlyButton(GameplayHudLayout_CircleButtonBounds(state));
    state->SquareButtonPressed = GameplayHudLayout_InputOnlyButton(GameplayHudLayout_SquareButtonBounds(state));
    state->TriangleButtonPressed = GameplayHudLayout_InputOnlyButton(GameplayHudLayout_TriangleButtonBounds(state));
}

#ifdef __cplusplus
}
#endif

#endif // GAMEPLAY_HUD_LAYOUT_H
