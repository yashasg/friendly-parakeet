#pragma once

#include <raylib.h>

// Runtime text assets kept in registry context.
struct TextContext {
    Font font_small{};
    Font font_medium{};
    Font font_large{};
    bool loaded = false;
};
