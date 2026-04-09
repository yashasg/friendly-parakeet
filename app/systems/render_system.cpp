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
#include <SDL.h>
#include <cmath>

static void draw_shape(SDL_Renderer* r, Shape shape, float cx, float cy, float size) {
    switch (shape) {
        case Shape::Circle: {
            // Approximate circle with 12-segment polygon
            constexpr int SEGS = 12;
            float radius = size / 2.0f;
            for (int i = 0; i < SEGS; ++i) {
                float a1 = static_cast<float>(i) / SEGS * 2.0f * 3.14159f;
                float a2 = static_cast<float>(i + 1) / SEGS * 2.0f * 3.14159f;
                SDL_RenderDrawLineF(r,
                    cx + std::cos(a1) * radius, cy + std::sin(a1) * radius,
                    cx + std::cos(a2) * radius, cy + std::sin(a2) * radius);
            }
            // Fill with small rects (simple fill approach)
            for (float dy = -radius; dy <= radius; dy += 2.0f) {
                float half_w = std::sqrt(radius * radius - dy * dy);
                SDL_FRect line = { cx - half_w, cy + dy, half_w * 2.0f, 2.0f };
                SDL_RenderFillRectF(r, &line);
            }
            break;
        }
        case Shape::Square: {
            float half = size / 2.0f;
            SDL_FRect rect = { cx - half, cy - half, size, size };
            SDL_RenderFillRectF(r, &rect);
            break;
        }
        case Shape::Triangle: {
            float half = size / 2.0f;
            // Fill triangle with horizontal lines
            for (float row = 0; row < size; row += 2.0f) {
                float ratio = row / size;
                float half_w = half * ratio;
                SDL_FRect line = { cx - half_w, cy - half + row, half_w * 2.0f, 2.0f };
                SDL_RenderFillRectF(r, &line);
            }
            break;
        }
    }
}

void render_system(entt::registry& reg, SDL_Renderer* renderer, float /*alpha*/) {
    auto& gs = reg.ctx().get<GameState>();
    auto& text_ctx = reg.ctx().get<TextContext>();

    // Clear
    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_RenderClear(renderer);

    // ── Draw lane lines ─────────────────────────────────────
    SDL_SetRenderDrawColor(renderer, 40, 40, 60, 255);
    for (int i = 0; i < constants::LANE_COUNT; ++i) {
        float lx = constants::LANE_X[i];
        SDL_RenderDrawLineF(renderer, lx, 0, lx,
            constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT);
    }

    if (gs.phase == GamePhase::Title) {
        // Title screen
        SDL_SetRenderDrawColor(renderer, 80, 180, 255, 255);
        // "SHAPESHIFTER" placeholder — draw 3 shapes as logo
        draw_shape(renderer, Shape::Circle,   280, 400, 80);
        draw_shape(renderer, Shape::Square,   360, 400, 80);
        SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255);
        draw_shape(renderer, Shape::Triangle, 440, 400, 80);

        // Title text
        text_draw(text_ctx, renderer, "SHAPESHIFTER",
            360, 500, 2, 80, 180, 255, 255,
            TextAlign::Center);

        // "TAP TO START" indicator — pulsing text
        float pulse = (std::sin(gs.phase_timer * 3.0f) + 1.0f) / 2.0f;
        uint8_t alpha = static_cast<uint8_t>(100 + pulse * 155);
        text_draw(text_ctx, renderer, "TAP TO START",
            360, 600, 1, 200, 200, 200, alpha,
            TextAlign::Center);

        SDL_RenderPresent(renderer);
        return;
    }

    // ── Draw obstacles ──────────────────────────────────────
    {
        auto view = reg.view<ObstacleTag, Position, Obstacle, Color, DrawSize>();
        for (auto [entity, pos, obs, col, dsz] : view.each()) {

            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);

            switch (obs.kind) {
                case ObstacleKind::ShapeGate: {
                    // Full-width gate with shape hole
                    SDL_FRect left  = { 0, pos.y, pos.x - 50, dsz.h };
                    SDL_FRect right = { pos.x + 50, pos.y,
                        constants::SCREEN_W - pos.x - 50, dsz.h };
                    SDL_RenderFillRectF(renderer, &left);
                    SDL_RenderFillRectF(renderer, &right);
                    // Draw shape indicator in the gap
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req) {
                        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, 120);
                        draw_shape(renderer, req->shape, pos.x, pos.y + dsz.h / 2, 40);
                    }
                    break;
                }
                case ObstacleKind::LaneBlock: {
                    auto* blocked = reg.try_get<BlockedLanes>(entity);
                    if (blocked) {
                        for (int i = 0; i < 3; ++i) {
                            if ((blocked->mask >> i) & 1) {
                                float lx = constants::LANE_X[i] - dsz.w / 2;
                                SDL_FRect r = { lx, pos.y, dsz.w, dsz.h };
                                SDL_RenderFillRectF(renderer, &r);
                            }
                        }
                    }
                    break;
                }
                case ObstacleKind::LowBar:
                case ObstacleKind::HighBar: {
                    SDL_FRect bar = { 0, pos.y, float(constants::SCREEN_W), dsz.h };
                    SDL_RenderFillRectF(renderer, &bar);
                    break;
                }
                case ObstacleKind::ComboGate: {
                    // Draw blocked lanes + shape indicator
                    auto* blocked = reg.try_get<BlockedLanes>(entity);
                    if (blocked) {
                        for (int i = 0; i < 3; ++i) {
                            if ((blocked->mask >> i) & 1) {
                                float lx = constants::LANE_X[i] - 120;
                                SDL_FRect r = { lx, pos.y, 240.0f, dsz.h };
                                SDL_RenderFillRectF(renderer, &r);
                            }
                        }
                    }
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req) {
                        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180);
                        // Find open lane
                        int open = 1;
                        if (blocked) {
                            for (int i = 0; i < 3; ++i) {
                                if (!((blocked->mask >> i) & 1)) { open = i; break; }
                            }
                        }
                        draw_shape(renderer, req->shape,
                            constants::LANE_X[open], pos.y + dsz.h / 2, 30);
                    }
                    break;
                }
                case ObstacleKind::SplitPath: {
                    // All lanes blocked except required lane
                    auto* rlane = reg.try_get<RequiredLane>(entity);
                    for (int i = 0; i < 3; ++i) {
                        if (!rlane || i != rlane->lane) {
                            float lx = constants::LANE_X[i] - 120;
                            SDL_FRect r = { lx, pos.y, 240.0f, dsz.h };
                            SDL_RenderFillRectF(renderer, &r);
                        }
                    }
                    auto* req = reg.try_get<RequiredShape>(entity);
                    if (req && rlane) {
                        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180);
                        draw_shape(renderer, req->shape,
                            constants::LANE_X[rlane->lane], pos.y + dsz.h / 2, 30);
                    }
                    break;
                }
            }
        }
    }

    // ── Draw particles ──────────────────────────────────────
    {
        auto view = reg.view<ParticleTag, Position, ParticleData, Color>();
        for (auto [entity, pos, pdata, col] : view.each()) {
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
            float half = pdata.size / 2.0f;
            SDL_FRect r = { pos.x - half, pos.y - half, pdata.size, pdata.size };
            SDL_RenderFillRectF(renderer, &r);
        }
    }

    // ── Draw score popups ───────────────────────────────────
    {
        auto view = reg.view<ScorePopup, Position, Color, Lifetime>();
        for (auto [entity, popup, pos, col, life] : view.each()) {
            float alpha_ratio = life.remaining / life.max_time;
            auto popup_alpha = static_cast<uint8_t>(alpha_ratio * 255);
            float popup_scale = 1.5f + popup.tier * 0.5f;
            // Score popups use small font (0) for normal, medium (1) for big combos
            int popup_font = (popup_scale > 2.5f) ? 1 : 0;
            text_draw_number(text_ctx, renderer, popup.value,
                pos.x, pos.y, popup_font,
                col.r, col.g, col.b, popup_alpha);
        }
    }

    // ── Draw player ─────────────────────────────────────────
    {
        auto view = reg.view<PlayerTag, Position, PlayerShape, VerticalState, Color>();
        for (auto [entity, pos, pshape, vstate, col] : view.each()) {
            float draw_y = pos.y + vstate.y_offset;

            // Draw size modulated by slide
            float size = constants::PLAYER_SIZE;
            if (vstate.mode == VMode::Sliding) {
                size *= 0.5f; // squash when sliding
            }

            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
            draw_shape(renderer, pshape.current, pos.x, draw_y, size);
        }
    }

    // ── Draw HUD ────────────────────────────────────────────
    if (gs.phase == GamePhase::Playing || gs.phase == GamePhase::Paused) {
        auto& score   = reg.ctx().get<ScoreState>();
        auto& burnout = reg.ctx().get<BurnoutState>();
        auto& config  = reg.ctx().get<DifficultyConfig>();

        // Score
        text_draw_number(text_ctx, renderer, score.displayed_score,
            120, 20, 1, 255, 255, 255, 255);

        // High score
        text_draw_number(text_ctx, renderer, score.high_score,
            120, 50, 0, 150, 150, 150, 180);

        // Speed bar
        float speed_ratio = (config.speed_multiplier - 1.0f) / 2.0f;
        SDL_SetRenderDrawColor(renderer, 50, 50, 80, 200);
        SDL_FRect speed_bg = { 20, 80, 680, 8 };
        SDL_RenderFillRectF(renderer, &speed_bg);
        SDL_SetRenderDrawColor(renderer, 100, 200, 255, 255);
        SDL_FRect speed_fill = { 20, 80, 680 * speed_ratio, 8 };
        SDL_RenderFillRectF(renderer, &speed_fill);

        // Burnout meter
        SDL_SetRenderDrawColor(renderer, 30, 30, 50, 200);
        SDL_FRect bo_bg = { 60, constants::BURNOUT_BAR_Y, 600, constants::BURNOUT_BAR_H };
        SDL_RenderFillRectF(renderer, &bo_bg);

        // Burnout fill color by zone
        switch (burnout.zone) {
            case BurnoutZone::None:
            case BurnoutZone::Safe:
                SDL_SetRenderDrawColor(renderer, 80, 200, 80, 255);
                break;
            case BurnoutZone::Risky:
                SDL_SetRenderDrawColor(renderer, 255, 200, 50, 255);
                break;
            case BurnoutZone::Danger:
                SDL_SetRenderDrawColor(renderer, 255, 80, 30, 255);
                break;
            case BurnoutZone::Dead:
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                break;
        }
        SDL_FRect bo_fill = { 60, constants::BURNOUT_BAR_Y,
            600 * burnout.meter, constants::BURNOUT_BAR_H };
        SDL_RenderFillRectF(renderer, &bo_fill);

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
            if (is_active) {
                SDL_SetRenderDrawColor(renderer, 60, 60, 100, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 30, 30, 50, 200);
            }
            SDL_FRect btn = { bx, constants::BUTTON_Y, constants::BUTTON_W, constants::BUTTON_H };
            SDL_RenderFillRectF(renderer, &btn);

            // Button border
            if (is_active) {
                SDL_SetRenderDrawColor(renderer, 120, 180, 255, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 60, 60, 80, 255);
            }
            SDL_RenderDrawRectF(renderer, &btn);

            // Shape icon in button
            auto shape = static_cast<Shape>(i);
            if (is_active) {
                SDL_SetRenderDrawColor(renderer, 200, 230, 255, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 100, 100, 120, 200);
            }
            draw_shape(renderer, shape,
                bx + constants::BUTTON_W / 2,
                constants::BUTTON_Y + constants::BUTTON_H / 2,
                40);
        }

        // Divider line between game and button zone
        SDL_SetRenderDrawColor(renderer, 40, 40, 60, 200);
        float divider_y = constants::SCREEN_H * constants::SWIPE_ZONE_SPLIT;
        SDL_RenderDrawLineF(renderer, 0, divider_y, constants::SCREEN_W, divider_y);
    }

    // ── Game Over overlay ───────────────────────────────────
    if (gs.phase == GamePhase::GameOver) {
        auto& score = reg.ctx().get<ScoreState>();

        // Dim overlay
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 180);
        SDL_FRect overlay = { 0, 0,
            float(constants::SCREEN_W), float(constants::SCREEN_H) };
        SDL_RenderFillRectF(renderer, &overlay);

        // "GAME OVER" heading
        text_draw(text_ctx, renderer, "GAME OVER",
            360, 440, 2, 255, 80, 80, 255,
            TextAlign::Center);

        // Score display
        text_draw_number(text_ctx, renderer, score.score,
            360, 510, 1, 255, 255, 255, 255);

        // High score
        text_draw_number(text_ctx, renderer, score.high_score,
            360, 560, 0, 200, 200, 100, 255);

        // "TAP TO RETRY" indicator — pulsing text
        float pulse = (std::sin(gs.phase_timer * 3.0f) + 1.0f) / 2.0f;
        auto retry_alpha = static_cast<uint8_t>(80 + pulse * 175);
        text_draw(text_ctx, renderer, "TAP TO RETRY",
            360, 650, 1, 200, 200, 200, retry_alpha,
            TextAlign::Center);
    }

    // ── Pause overlay ───────────────────────────────────────
    if (gs.phase == GamePhase::Paused) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
        SDL_FRect overlay = { 0, 0,
            float(constants::SCREEN_W), float(constants::SCREEN_H) };
        SDL_RenderFillRectF(renderer, &overlay);

        text_draw(text_ctx, renderer, "PAUSED",
            360, 580, 2, 255, 255, 255, 255,
            TextAlign::Center);
    }

    SDL_RenderPresent(renderer);
}

// Audio system stub — SFX playback requires Mix_Chunk loading
void audio_system(entt::registry& reg) {
    auto& audio = reg.ctx().get<AudioQueue>();
    // In a full implementation, iterate audio.queue[0..count-1]
    // and call Mix_PlayChannel for each SFX
    audio_clear(audio);
}
