#pragma once

#if !defined(GL_GLEXT_PROTOTYPES)
#define GL_GLEXT_PROTOTYPES
#endif

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#if __has_include(<SDL2/SDL_mixer.h>)
#include <SDL2/SDL_mixer.h>
#endif
#if __has_include(<SDL2/SDL_ttf.h>)
#include <SDL2/SDL_ttf.h>
#endif
#elif __has_include(<SDL.h>)
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#if __has_include(<SDL_mixer.h>)
#include <SDL_mixer.h>
#endif
#if __has_include(<SDL_ttf.h>)
#include <SDL_ttf.h>
#endif
#else
#error "SDL2 headers not found. Ensure SDL2 dependency is installed."
#endif
