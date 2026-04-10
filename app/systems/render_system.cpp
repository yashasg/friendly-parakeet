#include "all_systems.h"
#include "../components/transform.h"
#include "../components/player.h"
#include "../components/obstacle.h"
#include "../components/obstacle_data.h"
#include "../components/rendering.h"
#include "../components/scoring.h"
#include "../components/burnout.h"
#include "../components/lifetime.h"
#include "../components/game_state.h"
#include "../components/difficulty.h"
#include "../components/particle.h"
#include "../components/audio.h"
#include "../constants.h"
#include "../text_renderer.h"
#include <raylib.h>
#include <cmath>

static void draw_shape(Shape shape, float cx, float cy, float size, Color color) {
    switch (shape) {
        case Shape::Circle: {
            float radius = size / 2.0f;
            DrawCircleV({cx, cy}, radius, color);
            break;
        }
        case Shape::Square: {
            float half = size / 2.0f;
            DrawRectangleRec({cx - half, cy - half, size, size}, color);
            break;
        }
        case Shape::Triangle: {
            float half = size / 2.0f;
            Vector2 v1 = {cx,        cy - half};
            Vector2 v2 = {cx - half, cy + half};
            Vector2 v3 = {cx + half, cy + half};
            DrawTriangle(v1, v2, v3, color);
            break;
        }
    }
}

void render_system(entt::registry& reg, float /*alpha*/) {
    auto& gs = reg.ctx().get<GameState>();
    auto& text_ctx = reg.ctx().get<TextContext>();

    // Clear
    ClearBackground({15, 15, 25, 255});

    // ── Draw lane lines ─────────────────────────────────────
    Color lane_color = {40, 40, 60, 255};
    for (int i = 0; i < constants::LANE_COUNT; ++i) {
        float lx = constants::LANE_X[i];
        DrawLineV({lx, 0}, {lx, constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT},
                  lane_color);
    }

    if (gs.phase == GamePhase::Title) {
        // Title screen
        Color title_shape_color = {80, 180, 255, 255};
        draw_shape(Shape::Circle,   280, 400, 80, title_shape_color);
        draw_shape(Shape::Square,   360, 400, 80, title_shape_color);
        Color green_color = {100, 255, 100, 255};
        draw_shape(Shape::Triangle, 440, 400, 80, green_color);

        // Title text
        text_draw(text_ctx, "SHAPESHIFTER",
            360, 500, FontSize::Large, 80, 180, 255, 255,
            TextAlign::Center);

        // "TAP TO START" indicator — pulsing text
        float pulse = (std::sin(gs.phase_timer * 3.0f) + 1.0f) / 2.0f;
        uint8_t alpha = static_cast<uint8_t>(100 + pulse * 155);
        text_draw(text_ctx, "TAP TO START",
            360, 600, FontSize::Medium, 200, 200, 200, alpha,
            TextAlign::Center);

        return;
    }

    // ── Draw obstacles ──────────────────────────────────────
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, DrawColor, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {

            Color c = {col.r, col.g, col.b, col.a};

            switch (obs.kind) {
                case ObstacleKind::ShapeGate: {
                    // Full-width gate with shape hole
                    DrawRectangleRec({0, pos.y, pos.x - 50, dsz.h}, c);
                    DrawRectangleRec({pos.x + 50, pos.y,
                        constants::SCREEN_W - pos.x - 50, dsz.h}, c);
                    // Draw shape indicator in the gap
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req) {
                        Color ghost = {col.r, col.g, col.b, 120};
                        draw_shape(req->shape, pos.x, pos.y + dsz.h / 2, 40, ghost);
                    }
                    break;
                }
                case ObstacleKind::LaneBlock: {
                    auto* blocked = reg.try_get<BlockedLanes>(entity);
                    if (blocked) {
                        for (int i = 0; i < 3; ++i) {
                            if ((blocked->mask >> i) & 1) {
                                float lx = constants::LANE_X[i] - dsz.w / 2;
                                DrawRectangleRec({lx, pos.y, dsz.w, dsz.h}, c);
                            }
                        }
                    }
                    break;
                }
                case ObstacleKind::LowBar:
                case ObstacleKind::HighBar: {
                    DrawRectangleRec({0, pos.y, float(constants::SCREEN_W), dsz.h}, c);
                    break;
                }
                case ObstacleKind::ComboGate: {
                    auto* blocked = reg.try_get<BlockedLanes>(entity);
                    if (blocked) {
                        for (int i = 0; i < 3; ++i) {
                            if ((blocked->mask >> i) & 1) {
                                float lx = constants::LANE_X[i] - 120;
                                DrawRectangleRec({lx, pos.y, 240.0f, dsz.h}, c);
                            }
                        }
                    }
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req) {
                        Color white_ghost = {255, 255, 255, 180};
                        int open = 1;
                        if (blocked) {
                            for (int i = 0; i < 3; ++i) {
                                if (!((blocked->mask >> i) & 1)) { open = i; break; }
                            }
                        }
                        draw_shape(req->shape,
                            constants::LANE_X[open], pos.y + dsz.h / 2, 30, white_ghost);
                    }
                    break;
                }
                case ObstacleKind::SplitPath: {
                    auto* rlane = reg.try_get<RequiredLane>(entity);
                    for (int i = 0; i < 3; ++i) {
                        if (!rlane || i != rlane->lane) {
                            float lx = constants::LANE_X[i] - 120;
                            DrawRectangleRec({lx, pos.y, 240.0f, dsz.h}, c);
                        }
                    }
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req && rlane) {
                        Color white_ghost = {255, 255, 255, 180};
                        draw_shape(req->shape,
                            constants::LANE_X[rlane->lane], pos.y + dsz.h / 2, 30, white_ghost);
                    }
                    break;
                }
            }
        }
    }

    // ── Draw particles ──────────────────────────────────────
    {
        auto view = reg.view<ParticleTag, Position, ParticleData, DrawColor, Lifetime>();
        for (auto [entity, pos, pdata, col, life] : view.each()) {
            float ratio = (life.max_time > 0.0f) ? (life.remaining / life.max_time) : 1.0f;
            float draw_size = pdata.size * ratio;
            float half = draw_size / 2.0f;
            DrawRectangleRec({pos.x - half, pos.y - half, draw_size, draw_size},
                             {col.r, col.g, col.b, col.a});
        }
    }

    // ── Draw score popups ───────────────────────────────────
    {
        auto view = reg.view<ScorePopup, Position, DrawColor, Lifetime>();
        for (auto [entity, popup, pos, col, life] : view.each()) {
            float alpha_ratio = life.remaining / life.max_time;
            auto popup_alpha = static_cast<uint8_t>(alpha_ratio * 255);
            float popup_scale = 1.5f + popup.tier * 0.5f;
            FontSize popup_font = (popup_scale > 2.5f) ? FontSize::Medium : FontSize::Small;
            text_draw_number(text_ctx, popup.value,
                pos.x, pos.y, popup_font,
                col.r, col.g, col.b, popup_alpha);
        }
    }

    // ── Draw player ─────────────────────────────────────────
    {
        auto view = reg.view<PlayerTag, Position, PlayerShape, VerticalState, DrawColor>();
        for (auto [entity, pos, pshape, vstate, col] : view.each()) {
            float draw_y = pos.y + vstate.y_offset;

            float size = constants::PLAYER_SIZE;
            if (vstate.mode == VMode::Sliding) {
                size *= 0.5f;
            }

            Color pc = {col.r, col.g, col.b, col.a};
            draw_shape(pshape.current, pos.x, draw_y, size, pc);
        }
    }

    // ── Draw HUD ────────────────────────────────────────────
    if (gs.phase == GamePhase::Playing || gs.phase == GamePhase::Paused) {
        auto& score   = reg.ctx().get<ScoreState>();
        auto& burnout = reg.ctx().get<BurnoutState>();
        auto& config  = reg.ctx().get<DifficultyConfig>();

        // Score
        text_draw_number(text_ctx, score.displayed_score,
            120, 20, FontSize::Medium, 255, 255, 255, 255);

        // High score
        text_draw_number(text_ctx, score.high_score,
            120, 50, FontSize::Small, 150, 150, 150, 180);

        // Speed bar
        float speed_ratio = (config.speed_multiplier - 1.0f) / 2.0f;
        DrawRectangleRec({20, 80, 680, 8}, {50, 50, 80, 200});
        DrawRectangleRec({20, 80, 680 * speed_ratio, 8}, {100, 200, 255, 255});

        // Burnout meter
        DrawRectangleRec({60, constants::BURNOUT_BAR_Y, 600, constants::BURNOUT_BAR_H},
                         {30, 30, 50, 200});

        // Burnout fill color by zone
        Color bo_color;
        switch (burnout.zone) {
            case BurnoutZone::None:
            case BurnoutZone::Safe:
                bo_color = {80, 200, 80, 255};
                break;
            case BurnoutZone::Risky:
                bo_color = {255, 200, 50, 255};
                break;
            case BurnoutZone::Danger:
                bo_color = {255, 80, 30, 255};
                break;
            case BurnoutZone::Dead:
                bo_color = {255, 0, 0, 255};
                break;
        }
        DrawRectangleRec({60, constants::BURNOUT_BAR_Y,
            600 * burnout.meter, constants::BURNOUT_BAR_H}, bo_color);

        // Shape buttons
        float btn_area_x = (constants::SCREEN_W
            - 3 * constants::BUTTON_W
            - 2 * constants::BUTTON_SPACING) / 2.0f;

        Shape active_shape = Shape::Circle;
        auto pview = reg.view<PlayerTag, PlayerShape>();
        for (auto [e, ps] : pview.each()) {
            active_shape = ps.current;
        }

        for (int i = 0; i < 3; ++i) {
            float bx = btn_area_x
                + static_cast<float>(i) * (constants::BUTTON_W + constants::BUTTON_SPACING);
            bool is_active = (static_cast<int>(active_shape) == i);

            // Button background
            Color btn_bg = is_active ? Color{60, 60, 100, 255} : Color{30, 30, 50, 200};
            Rectangle btn = {bx, constants::BUTTON_Y, constants::BUTTON_W, constants::BUTTON_H};
            DrawRectangleRec(btn, btn_bg);

            // Button border
            Color btn_border = is_active ? Color{120, 180, 255, 255} : Color{60, 60, 80, 255};
            DrawRectangleLinesEx(btn, 1.0f, btn_border);

            // Shape icon in button
            auto shape = static_cast<Shape>(i);
            Color icon_color = is_active ? Color{200, 230, 255, 255} : Color{100, 100, 120, 200};
            draw_shape(shape,
                bx + constants::BUTTON_W / 2,
                constants::BUTTON_Y + constants::BUTTON_H / 2,
                40, icon_color);
        }

        // Divider line between game and button zone
        float divider_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;
        DrawLineV({0, divider_y}, {static_cast<float>(constants::SCREEN_W), divider_y},
                  {40, 40, 60, 200});
    }

    // ── Game Over overlay ───────────────────────────────────
    if (gs.phase == GamePhase::GameOver) {
        auto& score = reg.ctx().get<ScoreState>();

        // Dim overlay
        DrawRectangleRec({0, 0, float(constants::SCREEN_W), float(constants::SCREEN_H)},
                         {0, 0, 0, 180});

        // "GAME OVER" heading
        text_draw(text_ctx, "GAME OVER",
            360, 440, FontSize::Large, 255, 80, 80, 255,
            TextAlign::Center);

        // Score display
        text_draw_number(text_ctx, score.score,
            360, 510, FontSize::Medium, 255, 255, 255, 255);

        // High score
        text_draw_number(text_ctx, score.high_score,
            360, 560, FontSize::Small, 200, 200, 100, 255);

        // "TAP TO RETRY" indicator — pulsing text
        float pulse = (std::sin(gs.phase_timer * 3.0f) + 1.0f) / 2.0f;
        auto retry_alpha = static_cast<uint8_t>(80 + pulse * 175);
        text_draw(text_ctx, "TAP TO RETRY",
            360, 650, FontSize::Medium, 200, 200, 200, retry_alpha,
            TextAlign::Center);
    }

    // ── Pause overlay ───────────────────────────────────────
    if (gs.phase == GamePhase::Paused) {
        DrawRectangleRec({0, 0, float(constants::SCREEN_W), float(constants::SCREEN_H)},
                         {0, 0, 0, 160});

        text_draw(text_ctx, "PAUSED",
            360, 580, FontSize::Large, 255, 255, 255, 255,
            TextAlign::Center);
    }
}

// Audio system stub — SFX playback requires sound loading
void audio_system(entt::registry& reg) {
    auto& audio = reg.ctx().get<AudioQueue>();
    // In a full implementation, iterate audio.queue[0..count-1]
    // and call PlaySound for each SFX
    audio_clear(audio);
}
