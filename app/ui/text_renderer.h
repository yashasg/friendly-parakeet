#pragma once

#include "../components/text.h"

// ── Free functions ───────────────────────────────────────────

// Load fonts from default search paths (exe dir, local content, system fonts).
// Returns true on success.
bool text_init_default(TextContext& ctx);

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
