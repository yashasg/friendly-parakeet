#include "all_systems.h"
#include "../components/input.h"
#include "../components/game_state.h"
#include "../constants.h"
#include <SDL.h>

void input_system(entt::registry& reg, float raw_dt) {
    auto& input = reg.ctx().get<InputState>();
    clear_input_events(input);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                input.quit_requested = true;
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

#ifdef PLATFORM_DESKTOP
            // Keyboard events — desktop only.
            // event.key.repeat != 0 means the OS key-repeat is firing;
            // we only want the initial press so each key fires exactly once,
            // matching the one-gesture-per-swipe model.
            case SDL_KEYDOWN:
                if (event.key.repeat == 0) {
                    switch (event.key.keysym.sym) {
                        case SDLK_w: input.key_w = true; break;
                        case SDLK_a: input.key_a = true; break;
                        case SDLK_s: input.key_s = true; break;
                        case SDLK_d: input.key_d = true; break;
                        case SDLK_1: input.key_1 = true; break;
                        case SDLK_2: input.key_2 = true; break;
                        case SDLK_3: input.key_3 = true; break;
                        default: break;
                    }
                }
                break;
#endif

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
}
