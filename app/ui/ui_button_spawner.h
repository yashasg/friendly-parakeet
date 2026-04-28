#pragma once

#include <entt/entt.hpp>
#include "../components/input_events.h"
#include "../components/transform.h"
#include "../components/game_state.h"
#include "../constants.h"

// ---------------------------------------------------------------------------
// UI button entity spawning helpers.
//
// Called from game_state_system during phase transitions.  Each function
// stamps out lightweight button entities carrying UIPosition + HitBox +
// MenuAction + ActiveInPhase so the hit-test system can do generic
// point-in-rect tests instead of hard-coded coordinate checks.
// ---------------------------------------------------------------------------

// Destroy all UI button entities (call on phase transitions)
inline void destroy_ui_buttons(entt::registry& reg) {
    {   // Shape buttons
        auto sv = reg.view<ShapeButtonTag>();
        reg.destroy(sv.begin(), sv.end());
    }
    {   // Menu buttons
        auto mv = reg.view<MenuButtonTag>();
        reg.destroy(mv.begin(), mv.end());
    }
}

// Spawn end-screen buttons (GameOver / SongComplete)
//
// Coordinates derived from game_state_system.cpp:
//   BTN_W=280, BTN_H=50, BTN_GAP=15, BTN_PAD=10
//   btn_y1=870, btn_y2=935, btn_y3=1000
inline void spawn_end_screen_buttons(entt::registry& reg) {
    constexpr float BTN_W   = 280.0f;
    constexpr float BTN_H   =  50.0f;
    constexpr float BTN_PAD =  10.0f;

    const float btn_x_center = constants::SCREEN_W / 2.0f;

    // Center-Y of each button (top + half height)
    constexpr float btn_y_centers[3] = { 895.0f, 960.0f, 1025.0f };

    constexpr MenuActionKind actions[3] = {
        MenuActionKind::Restart,
        MenuActionKind::GoLevelSelect,
        MenuActionKind::GoMainMenu,
    };

    const GamePhaseBit mask = GamePhaseBit::GameOver
                           | GamePhaseBit::SongComplete;

    for (int i = 0; i < 3; ++i) {
        auto btn = reg.create();
        reg.emplace<MenuButtonTag>(btn);
        reg.emplace<UIPosition>(btn, Vector2{btn_x_center, btn_y_centers[i]});
        reg.emplace<HitBox>(btn, (BTN_W + BTN_PAD * 2) / 2.0f,
                                 (BTN_H + BTN_PAD * 2) / 2.0f);
        reg.emplace<MenuAction>(btn, actions[i], uint8_t{0});
        reg.emplace<ActiveInPhase>(btn, mask);
    }
}

// Spawn title screen tap areas
//
// A full-screen Confirm tap (→ LevelSelect) plus, on non-web builds,
// a small Exit button at the bottom.
inline void spawn_title_buttons(entt::registry& reg) {
#ifndef PLATFORM_WEB
    // Exit button (bottom centre).
    // game_state_system.cpp: EXIT_W=200, EXIT_H=50, EXIT_Y=1050
    // centre-Y = 1050 + 25 = 1075
    constexpr float EXIT_W = 200.0f;
    constexpr float EXIT_H =  50.0f;
    constexpr float EXIT_CENTER_Y = 1075.0f;
    constexpr float EXIT_TOP = EXIT_CENTER_Y - EXIT_H / 2.0f;

    // Full-screen tap → confirm (go to level select), excluding Exit region.
    // Shrink by 1px so the confirm region ends at EXIT_TOP - 1 and does not
    // overlap the Exit button whose top edge is at EXIT_TOP.
    constexpr float CONFIRM_HALF = (EXIT_TOP - 1.0f) / 2.0f;
    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<UIPosition>(btn, Vector2{constants::SCREEN_W / 2.0f, CONFIRM_HALF});
    reg.emplace<HitBox>(btn, constants::SCREEN_W / 2.0f, CONFIRM_HALF);
    reg.emplace<MenuAction>(btn, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<ActiveInPhase>(btn, GamePhaseBit::Title);

    auto exit_btn = reg.create();
    reg.emplace<MenuButtonTag>(exit_btn);
    reg.emplace<UIPosition>(exit_btn, Vector2{constants::SCREEN_W / 2.0f, EXIT_CENTER_Y});
    reg.emplace<HitBox>(exit_btn, EXIT_W / 2.0f, EXIT_H / 2.0f);
    reg.emplace<MenuAction>(exit_btn, MenuActionKind::Exit, uint8_t{0});
    reg.emplace<ActiveInPhase>(exit_btn, GamePhaseBit::Title);
#else
    // On web, no Exit button — full-screen Confirm
    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<UIPosition>(btn, Vector2{constants::SCREEN_W / 2.0f,
                                         constants::SCREEN_H / 2.0f});
    reg.emplace<HitBox>(btn, constants::SCREEN_W / 2.0f,
                              constants::SCREEN_H / 2.0f);
    reg.emplace<MenuAction>(btn, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<ActiveInPhase>(btn, GamePhaseBit::Title);
#endif
}

// Spawn pause overlay (full-screen tap → resume)
inline void spawn_pause_button(entt::registry& reg) {
    auto btn = reg.create();
    reg.emplace<MenuButtonTag>(btn);
    reg.emplace<UIPosition>(btn, Vector2{constants::SCREEN_W / 2.0f,
                                         constants::SCREEN_H / 2.0f});
    reg.emplace<HitBox>(btn, constants::SCREEN_W / 2.0f,
                              constants::SCREEN_H / 2.0f);
    reg.emplace<MenuAction>(btn, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<ActiveInPhase>(btn, GamePhaseBit::Paused);
}

// Spawn level-select UI entities (cards + difficulty buttons + start).
//
// Layout constants match level_select_controller.cpp exactly.
inline void spawn_level_select_buttons(entt::registry& reg) {
    constexpr float CARD_START_Y   = 200.0f;
    constexpr float CARD_HEIGHT    = 200.0f;
    constexpr float CARD_GAP       =  40.0f;
    constexpr float CARD_X         =  60.0f;
    constexpr float CARD_W         = 600.0f;

    constexpr float DIFF_BTN_W     = 160.0f;
    constexpr float DIFF_BTN_H     =  50.0f;
    constexpr float DIFF_BTN_Y_OFF = 120.0f;
    constexpr float DIFF_BTN_X0    = 100.0f;
    constexpr float DIFF_BTN_GAP   =  20.0f;

    constexpr float START_BTN_W    = 300.0f;
    constexpr float START_BTN_H    =  60.0f;
    constexpr float START_BTN_Y    = 1050.0f;  // top edge, from code

    constexpr float PAD = 10.0f;

    const GamePhaseBit mask = GamePhaseBit::LevelSelect;

    // 3 level cards
    for (int c = 0; c < LevelSelectState::LEVEL_COUNT; ++c) {
        float cy = CARD_START_Y
                 + static_cast<float>(c) * (CARD_HEIGHT + CARD_GAP)
                 + CARD_HEIGHT / 2.0f;
        auto card = reg.create();
        reg.emplace<MenuButtonTag>(card);
        reg.emplace<UIPosition>(card, Vector2{CARD_X + CARD_W / 2.0f, cy});
        reg.emplace<HitBox>(card, CARD_W / 2.0f, CARD_HEIGHT / 2.0f);
        reg.emplace<MenuAction>(card, MenuActionKind::SelectLevel,
                                static_cast<uint8_t>(c));
        reg.emplace<ActiveInPhase>(card, mask);
    }

    const auto* lss = reg.ctx().find<LevelSelectState>();
    const int selected_level = lss ? lss->selected_level : 0;
    const float selected_card_y = CARD_START_Y
        + static_cast<float>(selected_level) * (CARD_HEIGHT + CARD_GAP);

    // 3 difficulty buttons (positioned relative to current selected card)
    for (int d = 0; d < LevelSelectState::DIFFICULTY_COUNT; ++d) {
        float bx = DIFF_BTN_X0
                 + static_cast<float>(d) * (DIFF_BTN_W + DIFF_BTN_GAP)
                 + DIFF_BTN_W / 2.0f;
        float by = selected_card_y + DIFF_BTN_Y_OFF + DIFF_BTN_H / 2.0f;

        auto dbtn = reg.create();
        reg.emplace<MenuButtonTag>(dbtn);
        reg.emplace<UIPosition>(dbtn, Vector2{bx, by});
        reg.emplace<HitBox>(dbtn, (DIFF_BTN_W + PAD * 2) / 2.0f,
                                  (DIFF_BTN_H + PAD * 2) / 2.0f);
        reg.emplace<MenuAction>(dbtn, MenuActionKind::SelectDiff,
                                static_cast<uint8_t>(d));
        reg.emplace<ActiveInPhase>(dbtn, mask);
    }

    // Start button
    auto start = reg.create();
    reg.emplace<MenuButtonTag>(start);
    reg.emplace<UIPosition>(start, Vector2{constants::SCREEN_W / 2.0f,
                                           START_BTN_Y + START_BTN_H / 2.0f});
    reg.emplace<HitBox>(start, (START_BTN_W + PAD * 2) / 2.0f,
                                (START_BTN_H + PAD * 2) / 2.0f);
    reg.emplace<MenuAction>(start, MenuActionKind::Confirm, uint8_t{0});
    reg.emplace<ActiveInPhase>(start, mask);
}
