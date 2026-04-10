#pragma once

#include <raylib.h>
#include <cstdint>

// ── TextAlign ────────────────────────────────────────────────
enum class TextAlign : uint8_t { Left, Center, Right };

// ── FontSize ─────────────────────────────────────────────────
// Named constants for the pre-loaded font sizes.
enum class FontSize : int { Small = 0, Medium = 1, Large = 2 };

// ── TextContext ──────────────────────────────────────────────
// Plain data struct stored in the ECS registry context.
// Holds pre-loaded raylib Font objects at different point sizes.
// No logic, no methods beyond default construction.
struct TextContext {
    Font font_small{};    // ~16pt  — labels, small HUD text
    Font font_medium{};   // ~28pt  — HUD scores
    Font font_large{};    // ~48pt  — titles, GAME OVER
    bool loaded = false;  // true once fonts are successfully loaded
};

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
