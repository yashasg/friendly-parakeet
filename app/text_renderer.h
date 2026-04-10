#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <cstdint>

// ── TextAlign ────────────────────────────────────────────────
enum class TextAlign : uint8_t { Left, Center, Right };

// ── FontSize ─────────────────────────────────────────────────
// Named constants for the pre-loaded font sizes.
enum class FontSize : int { Small = 0, Medium = 1, Large = 2 };

// ── TextContext ──────────────────────────────────────────────
// Plain data struct stored in the ECS registry context.
// Holds pre-loaded TTF_Font pointers at different point sizes.
// No logic, no methods beyond default construction.
struct TextContext {
    TTF_Font* font_small  = nullptr;  // ~16pt  — labels, small HUD text
    TTF_Font* font_medium = nullptr;  // ~28pt  — HUD scores
    TTF_Font* font_large  = nullptr;  // ~48pt  — titles, GAME OVER
};

// ── Free functions ───────────────────────────────────────────

// Initialize SDL_ttf and load fonts at 3 sizes from the given path.
// Returns true on success.  On failure, TextContext fonts remain nullptr.
bool text_init(TextContext& ctx, const char* font_path);

// Close fonts and shut down SDL_ttf.
void text_shutdown(TextContext& ctx);

// Render a null-terminated string.
// font_size selects which pre-loaded font to use.
// (x, y) is the anchor point; alignment adjusts horizontally.
void text_draw(const TextContext& ctx,
               SDL_Renderer* renderer,
               const char* text,
               float x, float y,
               FontSize font_size,
               uint8_t r, uint8_t g, uint8_t b, uint8_t a,
               TextAlign align = TextAlign::Left);

// Convenience: format an integer and render it.
void text_draw_number(const TextContext& ctx,
                      SDL_Renderer* renderer,
                      int number,
                      float x, float y,
                      FontSize font_size,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                      TextAlign align = TextAlign::Center);

// Query the pixel width of a string at the given font size.
int text_width(const TextContext& ctx, const char* text, FontSize font_size);
