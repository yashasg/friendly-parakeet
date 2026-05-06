#include "all_systems.h"

#include "../components/render_tags.h"
#include "../components/transform.h"
#include "../components/registry_context.h"
#include "../constants.h"
#include "platform_state.h"

#include <SDL.h>
#include <cmath>

namespace {

void set_virtual_target(SDL_Renderer* renderer, SDL_Texture* target) {
    SDL_SetRenderTarget(renderer, target);
    const SDL_Rect viewport{0, 0, constants::SCREEN_W, constants::SCREEN_H};
    SDL_RenderSetViewport(renderer, &viewport);
}

void render_world_pass(entt::registry& reg, SDL_Renderer* renderer, SDL_Texture* target) {
    set_virtual_target(renderer, target);
    SDL_SetRenderDrawColor(renderer, 15, 15, 25, 255);
    SDL_RenderClear(renderer);
    game_render_system(reg, 0.0f);
}

void render_ui_pass(entt::registry& reg, SDL_Renderer* renderer, SDL_Texture* target) {
    set_virtual_target(renderer, target);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    ui_render_system(reg, 0.0f);
}

}  // namespace

void frame_render_system(entt::registry& reg) {
    SDL_Renderer* renderer = platform_state::renderer();
    if (renderer == nullptr) {
        game_render_system(reg, 0.0f);
        ui_render_system(reg, 0.0f);
        return;
    }

    const auto* targets = registry_ctx_find<RenderTargets>(reg);
    const auto* screen_transform = registry_ctx_find<ScreenTransform>(reg);
    const bool can_composite = targets != nullptr && screen_transform != nullptr && targets->ready();

    if (can_composite) {
        render_world_pass(reg, renderer, targets->world);
        render_ui_pass(reg, renderer, targets->ui);

        SDL_SetRenderTarget(renderer, nullptr);
        SDL_RenderSetViewport(renderer, nullptr);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        const SDL_Rect dst{
            static_cast<int>(std::lround(screen_transform->offset_x)),
            static_cast<int>(std::lround(screen_transform->offset_y)),
            static_cast<int>(std::lround(constants::SCREEN_W * screen_transform->scale)),
            static_cast<int>(std::lround(constants::SCREEN_H * screen_transform->scale)),
        };
        SDL_RenderCopy(renderer, targets->world, nullptr, &dst);
        SDL_RenderCopy(renderer, targets->ui, nullptr, &dst);
    } else {
        SDL_SetRenderTarget(renderer, nullptr);
        SDL_RenderSetViewport(renderer, nullptr);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        game_render_system(reg, 0.0f);
        ui_render_system(reg, 0.0f);
    }

    SDL_RenderPresent(renderer);
}
