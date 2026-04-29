/*******************************************************************************************
*
*   WindowCodegen v4.0 - tool description
*
*   LICENSE: Propietary License
*
*   Copyright (c) 2022 raylib technologies. All Rights Reserved.
*
*   Unauthorized copying of this file, via any medium is strictly prohibited
*   This project is proprietary and confidential unless the owner allows
*   usage in any other form by expresely written permission.
*
**********************************************************************************************/

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

//----------------------------------------------------------------------------------
// Controls Functions Declaration
//----------------------------------------------------------------------------------


//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main()
{
    // Initialization
    //---------------------------------------------------------------------------------------
    int screenWidth = 720;
    int screenHeight = 1280;

    InitWindow(screenWidth, screenHeight, "window_codegen");

    // window_codegen: controls initialization
    //----------------------------------------------------------------------------------
    // Define anchors
    Vector2 Anchor01 = { 0, 0 };            // ANCHOR ID:1
    
    // Define controls variables
    bool AudioOffsetMinusPressed = false;            // Button: AudioOffsetMinus
    bool AudioOffsetPlusPressed = false;            // Button: AudioOffsetPlus
    bool HapticsTogglePressed = false;            // Button: HapticsToggle
    bool ReduceMotionTogglePressed = false;            // Button: ReduceMotionToggle
    bool CloseButtonPressed = false;            // Button: CloseButton
    //----------------------------------------------------------------------------------

    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Implement required update logic
        //----------------------------------------------------------------------------------

        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR))); 

            // raygui: controls drawing
            //----------------------------------------------------------------------------------
            // Draw controls
            GuiLabel((Rectangle){ Anchor01.x + 210, Anchor01.y + 400, 300, 48 }, "SETTINGS");
            GuiLabel((Rectangle){ Anchor01.x + 144, Anchor01.y + 510, 200, 40 }, "Audio Offset");
            AudioOffsetMinusPressed = GuiButton((Rectangle){ Anchor01.x + 180, Anchor01.y + 560, 72, 77 }, "-"); 
            GuiLabel((Rectangle){ Anchor01.x + 252, Anchor01.y + 560, 216, 77 }, NULL);
            AudioOffsetPlusPressed = GuiButton((Rectangle){ Anchor01.x + 468, Anchor01.y + 560, 72, 77 }, "+"); 
            HapticsTogglePressed = GuiButton((Rectangle){ Anchor01.x + 152, Anchor01.y + 720, 416, 100 }, "HAPTICS: ON"); 
            GuiLabel((Rectangle){ Anchor01.x + 260, Anchor01.y + 829, 200, 32 }, NULL);
            ReduceMotionTogglePressed = GuiButton((Rectangle){ Anchor01.x + 152, Anchor01.y + 880, 416, 100 }, "MOTION: OFF"); 
            GuiLabel((Rectangle){ Anchor01.x + 260, Anchor01.y + 989, 200, 32 }, NULL);
            CloseButtonPressed = GuiButton((Rectangle){ Anchor01.x + 260, Anchor01.y + 1040, 200, 100 }, "BACK"); 
            //----------------------------------------------------------------------------------

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//------------------------------------------------------------------------------------
// Controls Functions Definitions (local)
//------------------------------------------------------------------------------------

