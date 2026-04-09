#include <SDL.h>
#include <entt/entt.hpp>

#include "constants.h"
#include "components/transform.h"
#include "components/player.h"
#include "components/obstacle.h"
#include "components/obstacle_data.h"
#include "components/input.h"
#include "components/game_state.h"
#include "components/scoring.h"
#include "components/burnout.h"
#include "components/difficulty.h"
#include "components/rendering.h"
#include "components/lifetime.h"
#include "components/particle.h"
#include "components/audio.h"
#include "systems/all_systems.h"

int main(int /*argc*/, char* /*argv*/[]) {

    // ── SDL INIT ─────────────────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "SHAPESHIFTER",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        constants::SCREEN_W / 2, constants::SCREEN_H / 2,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, constants::SCREEN_W, constants::SCREEN_H);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // ── ENTT REGISTRY ────────────────────────────────────────
    entt::registry reg;

    reg.ctx().emplace<InputState>();
    reg.ctx().emplace<GestureResult>();
    reg.ctx().emplace<ShapeButtonEvent>();
    reg.ctx().emplace<GameState>(GameState{
        .phase          = GamePhase::Title,
        .previous_phase = GamePhase::Title,
        .phase_timer    = 0.0f,
        .transition_pending = false,
        .next_phase     = GamePhase::Title,
        .transition_alpha = 0.0f
    });
    reg.ctx().emplace<ScoreState>();
    reg.ctx().emplace<BurnoutState>();
    reg.ctx().emplace<DifficultyConfig>();
    reg.ctx().emplace<AudioQueue>();

    // ── TIMING ───────────────────────────────────────────────
    constexpr float FIXED_DT  = 1.0f / 60.0f;
    constexpr float MAX_ACCUM = 0.1f;
    float accumulator = 0.0f;
    Uint64 prev_counter = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();

    bool running = true;

    // ── MAIN LOOP ────────────────────────────────────────────
    while (running) {

        // Delta time
        Uint64 now = SDL_GetPerformanceCounter();
        float raw_dt = static_cast<float>(now - prev_counter)
                     / static_cast<float>(freq);
        prev_counter = now;
        accumulator += raw_dt;
        if (accumulator > MAX_ACCUM) {
            accumulator = MAX_ACCUM;
        }

        // Event pump
        auto& input = reg.ctx().get<InputState>();
        input.clear_events();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;

                // Mouse events (desktop testing — mapped to touch)
                case SDL_MOUSEBUTTONDOWN:
                    input.touch_down = true;
                    input.touching   = true;
                    input.start_x    = static_cast<float>(event.button.x);
                    input.start_y    = static_cast<float>(event.button.y);
                    input.curr_x     = input.start_x;
                    input.curr_y     = input.start_y;
                    input.duration   = 0.0f;
                    break;
                case SDL_MOUSEBUTTONUP:
                    input.touch_up  = true;
                    input.touching  = false;
                    input.end_x     = static_cast<float>(event.button.x);
                    input.end_y     = static_cast<float>(event.button.y);
                    break;
                case SDL_MOUSEMOTION:
                    if (input.touching) {
                        input.curr_x = static_cast<float>(event.motion.x);
                        input.curr_y = static_cast<float>(event.motion.y);
                    }
                    break;

                // Touch events (mobile)
                case SDL_FINGERDOWN:
                    input.touch_down = true;
                    input.touching   = true;
                    input.start_x    = event.tfinger.x * constants::SCREEN_W;
                    input.start_y    = event.tfinger.y * constants::SCREEN_H;
                    input.curr_x     = input.start_x;
                    input.curr_y     = input.start_y;
                    input.duration   = 0.0f;
                    break;
                case SDL_FINGERUP:
                    input.touch_up  = true;
                    input.touching  = false;
                    input.end_x     = event.tfinger.x * constants::SCREEN_W;
                    input.end_y     = event.tfinger.y * constants::SCREEN_H;
                    break;
                case SDL_FINGERMOTION:
                    input.curr_x    = event.tfinger.x * constants::SCREEN_W;
                    input.curr_y    = event.tfinger.y * constants::SCREEN_H;
                    break;

                case SDL_APP_WILLENTERBACKGROUND:
                    if (reg.ctx().get<GameState>().phase == GamePhase::Playing) {
                        auto& gs = reg.ctx().get<GameState>();
                        gs.transition_pending = true;
                        gs.next_phase = GamePhase::Paused;
                    }
                    break;
            }
        }
        if (input.touching) {
            input.duration += raw_dt;
        }

        // Fixed timestep loop
        while (accumulator >= FIXED_DT) {
            // Phase 1: Input classification
            gesture_system(reg, FIXED_DT);

            // Phase 2: Game state
            game_state_system(reg, FIXED_DT);

            auto phase = reg.ctx().get<GameState>().phase;

            if (phase == GamePhase::Playing) {
                // Phase 3: Player
                player_action_system(reg, FIXED_DT);
                player_movement_system(reg, FIXED_DT);

                // Phase 4: World
                difficulty_system(reg, FIXED_DT);
                obstacle_spawn_system(reg, FIXED_DT);
                scroll_system(reg, FIXED_DT);
                burnout_system(reg, FIXED_DT);
                collision_system(reg, FIXED_DT);
                scoring_system(reg, FIXED_DT);
            }

            // Phase 5: Cleanup
            lifetime_system(reg, FIXED_DT);
            particle_system(reg, FIXED_DT);
            cleanup_system(reg, FIXED_DT);

            accumulator -= FIXED_DT;
        }

        // Render
        float alpha = accumulator / FIXED_DT;
        render_system(reg, renderer, alpha);

        // Audio
        audio_system(reg);
    }

    // ── SHUTDOWN ─────────────────────────────────────────────
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
