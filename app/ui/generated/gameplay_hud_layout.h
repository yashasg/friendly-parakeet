/*******************************************************************************************
*
*   Gameplay HUD Layout - Embeddable raygui Layout (NO main, NO RAYGUI_IMPLEMENTATION)
*
*   Source: content/ui/screens/gameplay.rgl
*   Template: tools/rguilayout/templates/embeddable_layout.h
*
*   NOTE: The dynamic energy bar, shape buttons, and approach rings are rendered by
*         draw_hud() in ui_render_system.cpp. This adapter provides only the
*         static Pause button overlay.
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
} GameplayHudLayoutState;

static inline GameplayHudLayoutState GameplayHudLayout_Init(void) {
    GameplayHudLayoutState state = {};
    state.Anchor01 = (Vector2){ 0, 0 };
    state.PauseButtonPressed = false;
    return state;
}

static inline void GameplayHudLayout_Render(GameplayHudLayoutState *state) {
    if (!state) return;
    state->PauseButtonPressed = GuiButton((Rectangle){ state->Anchor01.x + 620, state->Anchor01.y + 10, 80, 50 }, "||");
}

#ifdef __cplusplus
}
#endif

#endif // GAMEPLAY_HUD_LAYOUT_H
