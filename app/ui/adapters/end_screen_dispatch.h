// End-screen dispatch helper - consolidates game_over/song_complete button logic.

#ifndef END_SCREEN_DISPATCH_H
#define END_SCREEN_DISPATCH_H

#include "../../components/game_state.h"

// Generic end-screen choice dispatcher. Used by game_over and song_complete adapters.
// LayoutState must have RestartButtonPressed, LevelSelectButtonPressed, MenuButtonPressed fields.
template<typename LayoutState>
inline void dispatch_end_screen_choice(GameState& gs, const LayoutState& state) {
    if (state.RestartButtonPressed)
        gs.end_choice = EndScreenChoice::Restart;
    else if (state.LevelSelectButtonPressed)
        gs.end_choice = EndScreenChoice::LevelSelect;
    else if (state.MenuButtonPressed)
        gs.end_choice = EndScreenChoice::MainMenu;
}

#endif // END_SCREEN_DISPATCH_H
