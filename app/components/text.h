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
