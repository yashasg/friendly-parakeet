// rguilayout embeddable header template.
// Generated headers replace this sample metadata with screen-specific values.

#ifndef TITLE_LAYOUT_H
#define TITLE_LAYOUT_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------------
// Compile-time Layout Constants
//------------------------------------------------------------------------------------
#define TITLE_LAYOUT_WIDTH  720
#define TITLE_LAYOUT_HEIGHT 1280

//------------------------------------------------------------------------------------
// Layout State Structure (holds anchors and control state)
//------------------------------------------------------------------------------------
typedef struct TitleLayoutState {
    // Define anchors
    Vector2 Anchor01;            // ANCHOR ID:1
    
    // Define controls variables
    bool ExitButtonPressed;            // Button: ExitButton
    bool SettingsButtonPressed;            // Button: SettingsButton
} TitleLayoutState;

//------------------------------------------------------------------------------------
// API Functions
//------------------------------------------------------------------------------------

// Initialize layout state (call once)
TitleLayoutState TitleLayout_Init(void);

// Render layout controls (call every frame during BeginDrawing/EndDrawing)
// Returns: state updated with button press results
void TitleLayout_Render(TitleLayoutState *state);

#ifdef __cplusplus
}
#endif

#endif // TITLE_LAYOUT_H

//------------------------------------------------------------------------------------
// IMPLEMENTATION
//------------------------------------------------------------------------------------
#ifdef TITLE_LAYOUT_IMPLEMENTATION

#ifndef RAYGUI_H
    #error "raygui.h must be included BEFORE defining TITLE_LAYOUT_IMPLEMENTATION"
#endif

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------------------
// Initialize layout state
//------------------------------------------------------------------------------------
TitleLayoutState TitleLayout_Init(void) {
    TitleLayoutState state = {0};
    // Define anchors
    state.Anchor01 = (Vector2){ 0, 0 };            // ANCHOR ID:1
    
    // Define controls variables
    state.ExitButtonPressed = false;            // Button: ExitButton
    state.SettingsButtonPressed = false;            // Button: SettingsButton
    return state;
}

//------------------------------------------------------------------------------------
// Render layout
//------------------------------------------------------------------------------------
void TitleLayout_Render(TitleLayoutState *state) {
    if (!state) return;
    
    // Draw controls
    GuiLabel((Rectangle){ state->Anchor01.x + 160, state->Anchor01.y + 480, 400, 48 }, "SHAPESHIFTER");
    GuiLabel((Rectangle){ state->Anchor01.x + 220, state->Anchor01.y + 588, 280, 32 }, "TAP TO START");
    state->ExitButtonPressed = GuiButton((Rectangle){ state->Anchor01.x + 260, state->Anchor01.y + 1050, 200, 50 }, "EXIT"); 
    state->SettingsButtonPressed = GuiButton((Rectangle){ state->Anchor01.x + 20, state->Anchor01.y + 10, 80, 50 }, "SET"); 
}

#ifdef __cplusplus
}
#endif

#endif // TITLE_LAYOUT_IMPLEMENTATION
