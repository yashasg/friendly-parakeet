#pragma once

#include <entt/entt.hpp>

// Gameplay HUD bind system (issue #1297).
//
// Position-keyed bind for the dynamic-text slots in
// `content/ui/screens/gameplay.rgl`:
//   - ScoreSlot     — `%d` of `ScoreDisplay::displayed`
//   - HighScoreSlot — `BEST: %d` of `CurrentSongHighScore::value`
//   - ChainSlot     — `CHAIN %d[!]` of `ScoreState::chain_count` (empty
//                     when chain < 2). Also emplaces/removes
//                     `ChainBgPulseTag` for the meaningful-streak halo.
//   - EnergyLabel   — static text, only font-size/alpha bindings.
//
// Each slot also writes `UiLabelFontSize` + `UiLabelAlpha` so the
// generic label render pass picks the per-slot typography (the .rgl has
// no per-slot font-size field; these are screen-specific styling owned
// by the binder). Matches the position-keyed pattern established by
// `settings_screen_bind_system` and `game_over_scoreboard_bind_system`.
void gameplay_hud_bind_system(entt::registry& reg);
