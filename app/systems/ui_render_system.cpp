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
#include "../rendering/text_resources.h"
#include "../constants.h"

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
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

// ═════════════════════════════════════════════════════════════════════════════
// UI render system — 2D overlay pass
// ═════════════════════════════════════════════════════════════════════════════

namespace {

const Font& popup_font_for_size(const TextContext& ctx, FontSize font_size) {
    switch (font_size) {
        case FontSize::Small:  return ctx.font_small;
        case FontSize::Medium: return ctx.font_medium;
        case FontSize::Large:  return ctx.font_large;
    }
    return ctx.font_medium;
}

} // namespace

void ui_render_system(entt::registry& reg, float /*alpha*/) {
    auto& text_ctx = reg.ctx().get<TextContext>();
    const auto& gs = reg.ctx().get<GameState>();
    const auto& st = reg.ctx().get<ScreenTransform>();
    auto& ui_cam = ui_camera(reg).cam;

    ClearBackground(BLANK);
    BeginMode2D(ui_cam);

    // Popups
    {
        auto view = reg.view<PopupDisplay, ScreenPosition, TagHUDPass>();
        for (auto [entity, pd, sp] : view.each()) {
            (void)entity;
            if (!text_ctx.loaded || pd.text[0] == '\0') {
                continue;
            }

            const Font& font = popup_font_for_size(text_ctx, pd.font_size);
            const float font_size = static_cast<float>(font.baseSize);
            const float spacing = 1.0f;
            if (pd.measured_font_base_size != font.baseSize ||
                pd.measured_font_texture_id != font.texture.id) {
                const Vector2 size = MeasureTextEx(font, pd.text, font_size, spacing);
                pd.text_half_width = size.x / 2.0f;
                pd.measured_font_base_size = font.baseSize;
                pd.measured_font_texture_id = font.texture.id;
            }
            DrawTextEx(font, pd.text, {sp.x - pd.text_half_width, sp.y},
                       font_size, spacing, {pd.r, pd.g, pd.b, pd.a});
        }
    }

    // Overlay (if active)
    if (gs.phase == GamePhase::Paused) {
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
    switch (gs.phase) {
        case GamePhase::Title: {
            render_title_screen_ui(reg);
            break;
        }
        case GamePhase::LevelSelect: {
            render_level_select_screen_ui(reg);
            break;
        }
        case GamePhase::Playing: {
            render_gameplay_hud_screen_ui(reg);
            break;
        }
        case GamePhase::Paused: {
            render_paused_screen_ui(reg);
            break;
        }
        case GamePhase::GameOver: {
            render_game_over_screen_ui(reg);
            break;
        }
        case GamePhase::SongComplete: {
            render_song_complete_screen_ui(reg);
            break;
        }
        case GamePhase::Settings: {
            render_settings_screen_ui(reg);
            break;
        }
        case GamePhase::Tutorial: {
            render_tutorial_screen_ui(reg);
            break;
        }
    }

    // Restore raw mouse transform for non-UI systems in subsequent frames.
    SetMouseOffset(0, 0);
    SetMouseScale(1.0f, 1.0f);

    EndMode2D();
}
