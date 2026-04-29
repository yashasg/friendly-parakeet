#include "all_systems.h"
#include "camera_system.h"
#include "../components/transform.h"
#include "../components/player.h"
#include "../components/obstacle.h"
#include "../components/rendering.h"
#include "../components/scoring.h"
#include "../components/rhythm.h"
#include "../components/game_state.h"
#include "../components/song_state.h"
#include "../constants.h"
#include "../ui/text_renderer.h"
#include "../components/ui_state.h"

#include "../ui/screen_controllers/title_screen_controller.h"
#include "../ui/screen_controllers/paused_screen_controller.h"
#include "../ui/screen_controllers/game_over_screen_controller.h"
#include "../ui/screen_controllers/song_complete_screen_controller.h"
#include "../ui/screen_controllers/settings_screen_controller.h"
#include "../ui/screen_controllers/tutorial_screen_controller.h"
#include "../ui/screen_controllers/level_select_screen_controller.h"
#include "../ui/screen_controllers/gameplay_hud_screen_controller.h"
#include <raylib.h>
#include <raymath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

// ═════════════════════════════════════════════════════════════════════════════
// UI render system — 2D overlay pass
// ═════════════════════════════════════════════════════════════════════════════

void ui_render_system(entt::registry& reg, float /*alpha*/) {
    auto& text_ctx = reg.ctx().get<TextContext>();
    auto& ui = reg.ctx().get<UIState>();
    const auto& st = reg.ctx().get<ScreenTransform>();
    auto& ui_cam = ui_camera(reg).cam;

    ClearBackground(BLANK);
    BeginMode2D(ui_cam);

    // Popups
    {
        auto view = reg.view<PopupDisplay, ScreenPosition, TagHUDPass>();
        for (auto [entity, pd, sp] : view.each()) {
            text_draw(text_ctx, pd.text, sp.x, sp.y, pd.font_size,
                      pd.r, pd.g, pd.b, pd.a, TextAlign::Center);
        }
    }

    // Overlay (if active)
    if (ui.has_overlay) {
        DrawRectangleRec({0, 0, constants::SCREEN_W_F, constants::SCREEN_H_F},
                         {0, 0, 0, 160});
    }

    // UI rendering is handled exclusively by rguilayout screen controllers.

    // Make raygui hit-testing use virtual-space coordinates even when the
    // window is letterboxed/scaled relative to the fixed 720x1280 UI space.
    if (st.scale > 0.0f) {
        SetMouseOffset(static_cast<int>(std::lround(-st.offset_x)),
                       static_cast<int>(std::lround(-st.offset_y)));
        const float inv_scale = 1.0f / st.scale;
        SetMouseScale(inv_scale, inv_scale);
    }

    // Dynamic screens that still need specialized rendering
    switch (ui.active) {
        case ActiveScreen::Title: {
            render_title_screen_ui(reg);
            break;
        }
        case ActiveScreen::LevelSelect: {
            render_level_select_screen_ui(reg);
            break;
        }
        case ActiveScreen::Gameplay: {
            render_gameplay_hud_screen_ui(reg);
            break;
        }
        case ActiveScreen::Paused: {
            render_paused_screen_ui(reg);
            break;
        }
        case ActiveScreen::GameOver: {
            render_game_over_screen_ui(reg);
            break;
        }
        case ActiveScreen::SongComplete: {
            render_song_complete_screen_ui(reg);
            break;
        }
        case ActiveScreen::Settings: {
            render_settings_screen_ui(reg);
            break;
        }
        case ActiveScreen::Tutorial: {
            render_tutorial_screen_ui(reg);
            break;
        }
    }

    // Restore raw mouse transform for non-UI systems in subsequent frames.
    SetMouseOffset(0, 0);
    SetMouseScale(1.0f, 1.0f);

    EndMode2D();
}
