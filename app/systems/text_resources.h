#pragma once

#include <raylib.h>

// Authoritative runtime text-asset resource owner: holds the raylib Font
// handles for small/medium/large pre-loaded faces. Lives in registry context
// (not on an entity); destructor unloads the GPU font textures.
//
// Relocated out of `app/components/text_resources.h` (issue #1351) because
// this is a ctx-singleton RAII wrapper, not entity-owned plain data —
// `app/components/` stays reserved for atomic, queryable entity-owned tables
// per .squad/decisions.md §"app/components parallel audit verdict
// (consolidated)". UI/domain enums (FontSize) remain in
// `app/components/text.h`.

struct TextContext {
    Font font_small{};
    Font font_medium{};
    Font font_large{};
    bool loaded = false;

    TextContext() = default;
    TextContext(const TextContext&) = delete;
    TextContext& operator=(const TextContext&) = delete;
    TextContext(TextContext&& other) noexcept;
    TextContext& operator=(TextContext&& other) noexcept;
    ~TextContext();

    void release();
};

inline void TextContext::release() {
    if (IsFontValid(font_large)) {
        UnloadFont(font_large);
    }
    if (IsFontValid(font_medium)) {
        UnloadFont(font_medium);
    }
    if (IsFontValid(font_small)) {
        UnloadFont(font_small);
    }
    font_large = {};
    font_medium = {};
    font_small = {};
    loaded = false;
}

inline TextContext::~TextContext() { release(); }

inline TextContext::TextContext(TextContext&& other) noexcept
    : font_small{other.font_small},
      font_medium{other.font_medium},
      font_large{other.font_large},
      loaded{other.loaded}
{
    other.font_small = {};
    other.font_medium = {};
    other.font_large = {};
    other.loaded = false;
}

inline TextContext& TextContext::operator=(TextContext&& other) noexcept {
    if (this != &other) {
        release();
        font_small = other.font_small;
        font_medium = other.font_medium;
        font_large = other.font_large;
        loaded = other.loaded;
        other.font_small = {};
        other.font_medium = {};
        other.font_large = {};
        other.loaded = false;
    }
    return *this;
}
