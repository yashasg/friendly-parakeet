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
    bool RestartButtonPressed = false;            // Button: RestartButton
    bool LevelSelectButtonPressed = false;            // Button: LevelSelectButton
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
            GuiLabel((Rectangle){ Anchor01.x + 160, Anchor01.y + 340, 400, 60 }, "SONG COMPLETE");
            GuiLabel((Rectangle){ Anchor01.x + 260, Anchor01.y + 420, 200, 24 }, "SCORE");
            GuiLabel((Rectangle){ Anchor01.x + 260, Anchor01.y + 455, 200, 40 }, NULL);
            GuiLabel((Rectangle){ Anchor01.x + 210, Anchor01.y + 510, 300, 24 }, "HIGH SCORE");
            GuiLabel((Rectangle){ Anchor01.x + 210, Anchor01.y + 545, 300, 40 }, NULL);
            
            RestartButtonPressed = GuiButton((Rectangle){ Anchor01.x + 220, Anchor01.y + 870, 280, 50 }, "RESTART"); 
            LevelSelectButtonPressed = GuiButton((Rectangle){ Anchor01.x + 220, Anchor01.y + 935, 280, 50 }, "LEVEL SELECT"); 
            MenuButtonPressed = GuiButton((Rectangle){ Anchor01.x + 220, Anchor01.y + 1000, 280, 50 }, "MAIN MENU"); 
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

