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
    bool ContinueButtonPressed = false;            // Button: ContinueButton
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
            GuiLabel((Rectangle){ Anchor01.x + 160, Anchor01.y + 205, 400, 48 }, "QUICK START");
            GuiLabel((Rectangle){ Anchor01.x + 160, Anchor01.y + 333, 400, 40 }, "1. MATCH THE SHAPE");
            
            
            
            GuiLabel((Rectangle){ Anchor01.x + 110, Anchor01.y + 538, 500, 32 }, "BECOME THE SHAPE BEFORE IT HITS YOU");
            GuiLabel((Rectangle){ Anchor01.x + 160, Anchor01.y + 640, 400, 40 }, "2. DODGE LANES");
            GuiLabel((Rectangle){ Anchor01.x + 110, Anchor01.y + 710, 500, 32 }, "USE LEFT / RIGHT ARROW KEYS");
            GuiLabel((Rectangle){ Anchor01.x + 110, Anchor01.y + 710, 500, 32 }, "SWIPE LEFT OR RIGHT");
            GuiLabel((Rectangle){ Anchor01.x + 160, Anchor01.y + 806, 400, 40 }, "3. KEEP YOUR ENERGY");
            GuiLabel((Rectangle){ Anchor01.x + 110, Anchor01.y + 877, 500, 32 }, "MISSES DRAIN THE BAR ON THE LEFT");
            GuiLabel((Rectangle){ Anchor01.x + 110, Anchor01.y + 973, 500, 32 }, "CLEAR A SONG TO SET A HIGH SCORE");
            ContinueButtonPressed = GuiButton((Rectangle){ Anchor01.x + 180, Anchor01.y + 1075, 360, 102 }, "START"); 
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

