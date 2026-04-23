#pragma once

#include "../components/text.h"

// ── Free functions ───────────────────────────────────────────

// Load fonts at 3 sizes from the given path.
// Returns true on success.  On failure, TextContext fonts remain invalid.
bool text_init(TextContext& ctx, const char* font_path);

// Unload fonts.
void text_shutdown(TextContext& ctx);

// Render a null-terminated string.
// font_size selects which pre-loaded font to use.
// (x, y) is the anchor point; alignment adjusts horizontally.
void text_draw(const TextContext& ctx,
               const char* text,
               float x, float y,
               FontSize font_size,
               uint8_t r, uint8_t g, uint8_t b, uint8_t a,
               TextAlign align = TextAlign::Left);

// Convenience: format an integer and render it.
void text_draw_number(const TextContext& ctx,
                      int number,
                      float x, float y,
                      FontSize font_size,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                      TextAlign align = TextAlign::Center);

// Query the pixel width of a string at the given font size.
int text_width(const TextContext& ctx, const char* text, FontSize font_size);
