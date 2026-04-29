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
    bool ResumeButtonPressed = false;            // Button: ResumeButton
    bool MenuButtonPressed = false;            // Button: MenuButton
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
            GuiLabel((Rectangle){ Anchor01.x + 200, Anchor01.y + 440, 320, 60 }, "PAUSED");
            GuiLabel((Rectangle){ Anchor01.x + 110, Anchor01.y + 540, 500, 32 }, "TAP RESUME TO CONTINUE");
            ResumeButtonPressed = GuiButton((Rectangle){ Anchor01.x + 160, Anchor01.y + 620, 400, 100 }, "RESUME"); 
            GuiLabel((Rectangle){ Anchor01.x + 110, Anchor01.y + 760, 500, 32 }, "OR RETURN TO MAIN MENU");
            MenuButtonPressed = GuiButton((Rectangle){ Anchor01.x + 160, Anchor01.y + 820, 400, 100 }, "MAIN MENU"); 
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

