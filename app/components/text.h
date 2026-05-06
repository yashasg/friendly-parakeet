#pragma once

#include <cstdint>
#include <SDL_ttf.h>

// ── TextAlign ────────────────────────────────────────────────
enum class TextAlign : uint8_t { Left, Center, Right };

// ── FontSize ─────────────────────────────────────────────────
// Named constants for the pre-loaded font sizes.
enum class FontSize : int { Small = 0, Medium = 1, Large = 2 };

// ── TextContext ──────────────────────────────────────────────
// Plain data struct stored in the ECS registry context.
// Holds pre-loaded runtime Font objects at different point sizes.
// No logic, no methods beyond default construction.
struct TextContext {
    TTF_Font* font_small = nullptr;   // ~16pt  — labels, small HUD text
    TTF_Font* font_medium = nullptr;  // ~28pt  — HUD scores
    TTF_Font* font_large = nullptr;   // ~48pt  — titles, GAME OVER
    bool loaded = false;  // true once fonts are successfully loaded
};
