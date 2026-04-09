#include "text_renderer.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdio>

// ── Helper: pick font by size index ─────────────────────────
static TTF_Font* pick_font(const TextContext& ctx, int font_size) {
    switch (font_size) {
        case 0:  return ctx.font_small;
        case 1:  return ctx.font_medium;
        case 2:  return ctx.font_large;
        default: return ctx.font_medium;
    }
}

// ── text_init ───────────────────────────────────────────────
bool text_init(TextContext& ctx, const char* font_path) {
    if (TTF_Init() != 0) {
        SDL_Log("TTF_Init failed: %s", TTF_GetError());
        return false;
    }

    ctx.font_small  = TTF_OpenFont(font_path, 16);
    ctx.font_medium = TTF_OpenFont(font_path, 28);
    ctx.font_large  = TTF_OpenFont(font_path, 48);

    if (!ctx.font_small || !ctx.font_medium || !ctx.font_large) {
        SDL_Log("Failed to load font '%s': %s", font_path, TTF_GetError());
        text_shutdown(ctx);
        return false;
    }

    // Use LCD hinting for cleaner rendering at small sizes
    TTF_SetFontHinting(ctx.font_small,  TTF_HINTING_LIGHT);
    TTF_SetFontHinting(ctx.font_medium, TTF_HINTING_LIGHT);
    TTF_SetFontHinting(ctx.font_large,  TTF_HINTING_LIGHT);

    return true;
}

// ── text_shutdown ───────────────────────────────────────────
void text_shutdown(TextContext& ctx) {
    if (ctx.font_large)  { TTF_CloseFont(ctx.font_large);  ctx.font_large  = nullptr; }
    if (ctx.font_medium) { TTF_CloseFont(ctx.font_medium); ctx.font_medium = nullptr; }
    if (ctx.font_small)  { TTF_CloseFont(ctx.font_small);  ctx.font_small  = nullptr; }
    TTF_Quit();
}

// ── text_draw ───────────────────────────────────────────────
void text_draw(const TextContext& ctx,
               SDL_Renderer* renderer,
               const char* text,
               float x, float y,
               int font_size,
               uint8_t r, uint8_t g, uint8_t b, uint8_t a,
               TextAlign align) {
    if (!text || !text[0]) return;

    TTF_Font* font = pick_font(ctx, font_size);
    if (!font) return;

    SDL_Color color = { r, g, b, a };
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_FreeSurface(surface);
        return;
    }

    // Enable alpha blending on the texture
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    float w = static_cast<float>(surface->w);
    float h = static_cast<float>(surface->h);

    float draw_x = x;
    switch (align) {
        case TextAlign::Left:                         break;
        case TextAlign::Center: draw_x = x - w / 2;  break;
        case TextAlign::Right:  draw_x = x - w;      break;
    }

    SDL_FRect dst = { draw_x, y, w, h };
    SDL_RenderCopyF(renderer, texture, nullptr, &dst);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

// ── text_draw_number ────────────────────────────────────────
void text_draw_number(const TextContext& ctx,
                      SDL_Renderer* renderer,
                      int number,
                      float x, float y,
                      int font_size,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                      TextAlign align) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", number);
    text_draw(ctx, renderer, buf, x, y, font_size, r, g, b, a, align);
}

// ── text_width ──────────────────────────────────────────────
int text_width(const TextContext& ctx, const char* text, int font_size) {
    TTF_Font* font = pick_font(ctx, font_size);
    if (!font || !text) return 0;
    int w = 0, h = 0;
    TTF_SizeUTF8(font, text, &w, &h);
    return w;
}
