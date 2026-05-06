#include "text_renderer.h"
#include "../systems/platform_state.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <array>
#include <filesystem>
#include <string>

namespace {

struct FontSlot {
    TTF_Font* TextContext::*font_member;
    int size;
};

constexpr std::array<FontSlot, 3> kFontSlots{{
    {&TextContext::font_small, 16},
    {&TextContext::font_medium, 28},
    {&TextContext::font_large, 48},
}};

template <typename Context, typename Fn>
void for_each_font(Context& ctx, Fn&& fn) {
    for (const auto& slot : kFontSlots) {
        fn(ctx.*slot.font_member, slot);
    }
}

TTF_Font* pick_font(const TextContext& ctx, FontSize font_size) {
    constexpr std::size_t kDefaultFontIndex = static_cast<std::size_t>(FontSize::Medium);
    const int font_index = static_cast<int>(font_size);
    if (font_index < 0 || static_cast<std::size_t>(font_index) >= kFontSlots.size()) {
        return ctx.*kFontSlots[kDefaultFontIndex].font_member;
    }
    return ctx.*kFontSlots[static_cast<std::size_t>(font_index)].font_member;
}

bool is_font_loaded(TTF_Font* const& font) {
    return font != nullptr;
}

bool text_init_from_path(TextContext& ctx, const char* font_path) {
    if (!font_path) return false;
    if (ctx.loaded) {
        text_shutdown(ctx);
    }

    bool loaded_all_fonts = true;
    for_each_font(ctx, [font_path, &loaded_all_fonts](TTF_Font*& font, const FontSlot& slot) {
        font = TTF_OpenFont(font_path, slot.size);
        loaded_all_fonts = loaded_all_fonts && is_font_loaded(font);
    });

    if (!loaded_all_fonts) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to load font: %s", font_path);
        text_shutdown(ctx);
        return false;
    }

    ctx.loaded = true;
    return true;
}

}  // namespace

bool text_init_default(TextContext& ctx) {
    const std::array<std::filesystem::path, 6> font_paths = {{
        std::filesystem::current_path() / "content/fonts/LiberationMono-Regular.ttf",
        "content/fonts/LiberationMono-Regular.ttf",
        "/System/Library/Fonts/Supplemental/Menlo.ttc",
        "/System/Library/Fonts/Monaco.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    }};
    for (const auto& path : font_paths) {
        const std::string utf8_path = path.string();
        if (text_init_from_path(ctx, utf8_path.c_str())) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Loaded font: %s", utf8_path.c_str());
            return true;
        }
    }
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load any TTF font");
    return false;
}

void text_shutdown(TextContext& ctx) {
    for_each_font(ctx, [](TTF_Font*& font, const FontSlot&) {
        if (is_font_loaded(font)) {
            TTF_CloseFont(font);
            font = nullptr;
        }
    });
    ctx.loaded = false;
}

// ── text_draw ───────────────────────────────────────────────
void text_draw(const TextContext& ctx,
               const char* text,
               float x, float y,
               FontSize font_size,
               uint8_t r, uint8_t g, uint8_t b, uint8_t a,
               TextAlign align) {
    if (!text || !text[0] || !ctx.loaded) return;

    TTF_Font* font = pick_font(ctx, font_size);
    if (!font) return;

    int text_w = 0, text_h = 0;
    TTF_SizeUTF8(font, text, &text_w, &text_h);

    float draw_x = x;
    switch (align) {
        case TextAlign::Left:                                             break;
        case TextAlign::Center: draw_x = x - static_cast<float>(text_w) * 0.5f; break;
        case TextAlign::Right:  draw_x = x - static_cast<float>(text_w);        break;
    }

    SDL_Renderer* renderer = platform_state::renderer();
    if (!renderer) return;

    SDL_Color color{r, g, b, a};
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) return;

    const SDL_FRect dst{draw_x, y,
                        static_cast<float>(text_w), static_cast<float>(text_h)};
    SDL_RenderCopyF(renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
}
