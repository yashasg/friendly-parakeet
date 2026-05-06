#include "all_systems.h"
#include "../components/transform.h"
#include "../components/scoring.h"
#include "../components/game_state.h"
#include "../components/song_state.h"
#include "../components/registry_context.h"
#include "../constants.h"
#include "../ui/text_renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// ═════════════════════════════════════════════════════════════════════════════
// UI render system — 2D overlay pass
// ═════════════════════════════════════════════════════════════════════════════

namespace {

void draw_phase_banner(const TextContext& text_ctx, const char* line1, const char* line2 = nullptr) {
    text_draw(text_ctx, line1, constants::SCREEN_W_F * 0.5f, 90.0f,
              FontSize::Medium, 240, 240, 240, 255, TextAlign::Center);
    if (line2 != nullptr) {
        text_draw(text_ctx, line2, constants::SCREEN_W_F * 0.5f, 130.0f,
                  FontSize::Small, 200, 200, 200, 255, TextAlign::Center);
    }
}

} // namespace

void ui_render_system(entt::registry& reg, float /*alpha*/) {
    auto& text_ctx = registry_ctx_get<TextContext>(reg);
    const auto& gs = registry_ctx_get<GameState>(reg);

    render_api::clear_background(BLANK);

    // Popups
    {
        auto view = reg.view<PopupDisplay, ScreenPosition, TagHUDPass>();
        for (auto [entity, pd, sp] : view.each()) {
            (void)entity;
            text_draw(text_ctx, pd.text, sp.x, sp.y, pd.font_size,
                      pd.r, pd.g, pd.b, pd.a, TextAlign::Center);
        }
    }

    // Overlay (if active)
    if (gs.phase == GamePhase::Paused) {
        render_api::draw_rectangle_rec({0, 0, constants::SCREEN_W_F, constants::SCREEN_H_F},
                                               {0, 0, 0, 160});
    }

    switch (gs.phase) {
        case GamePhase::Title: {
            draw_phase_banner(text_ctx, "SHAPESHIFTER", "ENTER / TAP TO START");
            break;
        }
        case GamePhase::LevelSelect: {
            draw_phase_banner(text_ctx, "LEVEL SELECT", "ARROWS/SWIPE TO CHOOSE, ENTER/TAP TO PLAY");
            if (const auto* lss = registry_ctx_find<LevelSelectState>(reg); lss != nullptr) {
                const LevelInfo& level = selected_level_info(*lss);
                const char* difficulty = selected_difficulty_name(*lss);
                char line[128] = {};
                std::snprintf(line, sizeof(line), "%s  [%s]", level.title, difficulty);
                text_draw(text_ctx, line, constants::SCREEN_W_F * 0.5f, 180.0f,
                          FontSize::Medium, 255, 255, 255, 255, TextAlign::Center);
            }
            break;
        }
        case GamePhase::Playing: {
            if (const auto* score = registry_ctx_find<ScoreState>(reg); score != nullptr) {
                char score_line[64] = {};
                std::snprintf(score_line, sizeof(score_line), "SCORE %d  CHAIN %d",
                              score->displayed_score, score->chain_count);
                text_draw(text_ctx, score_line, 20.0f, 20.0f, FontSize::Small,
                          255, 255, 255, 255, TextAlign::Left);
            }
            if (const auto* energy = registry_ctx_find<EnergyState>(reg); energy != nullptr) {
                char energy_line[32] = {};
                const int pct = std::clamp(static_cast<int>(std::lround(energy->display * 100.0f)), 0, 100);
                std::snprintf(energy_line, sizeof(energy_line), "ENERGY %d%%", pct);
                text_draw(text_ctx, energy_line, constants::SCREEN_W_F - 20.0f, 20.0f, FontSize::Small,
                          120, 230, 255, 255, TextAlign::Right);
            }
            if (const auto* song = registry_ctx_find<SongState>(reg); song != nullptr && song->playing) {
                char beat_line[32] = {};
                std::snprintf(beat_line, sizeof(beat_line), "BEAT %d", song->current_beat);
                text_draw(text_ctx, beat_line, constants::SCREEN_W_F * 0.5f, 20.0f, FontSize::Small,
                          255, 220, 120, 255, TextAlign::Center);
            }
            break;
        }
        case GamePhase::Paused: {
            draw_phase_banner(text_ctx, "PAUSED", "ENTER / TAP TO RESUME");
            break;
        }
        case GamePhase::GameOver: {
            const char* reason = "MISSED BEAT";
            if (const auto* gos = registry_ctx_find<GameOverState>(reg); gos != nullptr) {
                switch (gos->cause) {
                    case DeathCause::EnergyDepleted: reason = "ENERGY DEPLETED"; break;
                    case DeathCause::HitABar: reason = "HIT A BAR"; break;
                    case DeathCause::MissedABeat:
                    case DeathCause::None:
                        break;
                }
            }
            draw_phase_banner(text_ctx, "GAME OVER", "ENTER / TAP TO RETRY");
            text_draw(text_ctx, reason, constants::SCREEN_W_F * 0.5f, 180.0f,
                      FontSize::Small, 255, 160, 160, 255, TextAlign::Center);
            break;
        }
        case GamePhase::SongComplete: {
            draw_phase_banner(text_ctx, "SONG COMPLETE", "ENTER / TAP TO RETRY");
            break;
        }
        case GamePhase::Settings: {
            draw_phase_banner(text_ctx, "SETTINGS", "ENTER / TAP TO RETURN");
            break;
        }
        case GamePhase::Tutorial: {
            draw_phase_banner(text_ctx, "TUTORIAL", "SWIPE OR ARROWS TO MOVE SHAPE");
            break;
        }
    }

}
