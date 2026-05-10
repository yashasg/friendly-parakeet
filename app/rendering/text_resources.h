#pragma once

#include <raylib.h>

// Authoritative runtime text asset state kept in registry context.
// UI/domain enums remain in app/components/text.h.
struct TextContext {
    Font font_small{};
    Font font_medium{};
    Font font_large{};
    bool loaded = false;
};
