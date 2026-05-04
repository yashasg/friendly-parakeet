#pragma once

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#elif __has_include(<SDL.h>)
#include <SDL.h>
#include <SDL_opengl.h>
#else
#error "SDL2 headers not found. Ensure SDL2 dependency is installed."
#endif
