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
    bool DifficultyEasyPressed = false;            // Button: DifficultyEasy
    bool DifficultyMediumPressed = false;            // Button: DifficultyMedium
    bool DifficultyHardPressed = false;            // Button: DifficultyHard
    bool StartButtonPressed = false;            // Button: StartButton
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
            GuiLabel((Rectangle){ Anchor01.x + 210, Anchor01.y + 80, 300, 48 }, "SELECT LEVEL");
            
            
            
            
            
            DifficultyEasyPressed = GuiButton((Rectangle){ Anchor01.x + 100, Anchor01.y + 1400, 160, 50 }, "EASY"); 
            DifficultyMediumPressed = GuiButton((Rectangle){ Anchor01.x + 280, Anchor01.y + 1400, 160, 50 }, "MEDIUM"); 
            DifficultyHardPressed = GuiButton((Rectangle){ Anchor01.x + 460, Anchor01.y + 1400, 160, 50 }, "HARD"); 
            StartButtonPressed = GuiButton((Rectangle){ Anchor01.x + 210, Anchor01.y + 1050, 300, 60 }, "START"); 
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

