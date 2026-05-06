#include <catch2/catch_test_macros.hpp>

#include <SDL.h>

TEST_CASE("sdl2 render target compositing loop is available",
          "[render][sdl2][validation]") {
    REQUIRE(SDL_Init(SDL_INIT_VIDEO) == 0);

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 64, 64, 32, SDL_PIXELFORMAT_RGBA32);
    REQUIRE(surface != nullptr);

    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface);
    REQUIRE(renderer != nullptr);
    if (!SDL_RenderTargetSupported(renderer)) {
        SDL_DestroyRenderer(renderer);
        SDL_FreeSurface(surface);
        SDL_Quit();
        SKIP("SDL renderer does not support target textures on this platform");
    }

    SDL_Texture* world = SDL_CreateTexture(renderer,
                                           SDL_PIXELFORMAT_RGBA8888,
                                           SDL_TEXTUREACCESS_TARGET,
                                           64,
                                           64);
    SDL_Texture* ui = SDL_CreateTexture(renderer,
                                        SDL_PIXELFORMAT_RGBA8888,
                                        SDL_TEXTUREACCESS_TARGET,
                                        64,
                                        64);
    REQUIRE(world != nullptr);
    REQUIRE(ui != nullptr);

    SDL_SetTextureBlendMode(world, SDL_BLENDMODE_BLEND);
    SDL_SetTextureBlendMode(ui, SDL_BLENDMODE_BLEND);

    REQUIRE(SDL_SetRenderTarget(renderer, world) == 0);
    SDL_SetRenderDrawColor(renderer, 10, 20, 30, 255);
    REQUIRE(SDL_RenderClear(renderer) == 0);

    REQUIRE(SDL_SetRenderTarget(renderer, ui) == 0);
    SDL_SetRenderDrawColor(renderer, 200, 120, 20, 120);
    REQUIRE(SDL_RenderClear(renderer) == 0);

    REQUIRE(SDL_SetRenderTarget(renderer, nullptr) == 0);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    REQUIRE(SDL_RenderClear(renderer) == 0);
    REQUIRE(SDL_RenderCopy(renderer, world, nullptr, nullptr) == 0);
    REQUIRE(SDL_RenderCopy(renderer, ui, nullptr, nullptr) == 0);
    SDL_RenderPresent(renderer);

    SDL_DestroyTexture(ui);
    SDL_DestroyTexture(world);
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    SDL_Quit();
}
