#include <SDL.h>
#include <entt/entt.hpp>

#include "version.h"
#include "constants.h"
#include "components/input.h"
#include "components/game_state.h"
#include "components/scoring.h"
#include "components/burnout.h"
#include "components/difficulty.h"
#include "components/audio.h"
#include "systems/all_systems.h"

int main(int /*argc*/, char* /*argv*/[]) {

    // ── SDL INIT ─────────────────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("SHAPESHIFTER v%s", SHAPESHIFTER_VERSION);

    std::string window_title = std::string("SHAPESHIFTER v") + SHAPESHIFTER_VERSION;
    SDL_Window* window = SDL_CreateWindow(
        window_title.c_str(),
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

    // ── MAIN LOOP ────────────────────────────────────────────
    while (true) {

        // Delta time
        Uint64 now = SDL_GetPerformanceCounter();
        float raw_dt = static_cast<float>(now - prev_counter)
                     / static_cast<float>(freq);
        prev_counter = now;
        accumulator += raw_dt;
        if (accumulator > MAX_ACCUM) {
            accumulator = MAX_ACCUM;
        }

        // Phase 0: Input (polls SDL events, updates InputState)
        input_system(reg, raw_dt);
        if (reg.ctx().get<InputState>().quit_requested) break;

        // Fixed timestep loop — all systems self-guard on GamePhase
        while (accumulator >= FIXED_DT) {
            gesture_system(reg, FIXED_DT);
            game_state_system(reg, FIXED_DT);
            player_action_system(reg, FIXED_DT);
            player_movement_system(reg, FIXED_DT);
            difficulty_system(reg, FIXED_DT);
            obstacle_spawn_system(reg, FIXED_DT);
            scroll_system(reg, FIXED_DT);
            burnout_system(reg, FIXED_DT);
            collision_system(reg, FIXED_DT);
            scoring_system(reg, FIXED_DT);
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
