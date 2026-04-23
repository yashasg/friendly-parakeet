#include "text_renderer.h"
#include <raylib.h>
#include <cstdio>

// ── Helper: pick font by size index ─────────────────────────
static const Font& pick_font(const TextContext& ctx, FontSize font_size) {
    switch (font_size) {
        case FontSize::Small:  return ctx.font_small;
        case FontSize::Medium: return ctx.font_medium;
        case FontSize::Large:  return ctx.font_large;
        default:               return ctx.font_medium;
    }
}

// ── text_init ───────────────────────────────────────────────
bool text_init(TextContext& ctx, const char* font_path) {
    if (!FileExists(font_path)) {
        TraceLog(LOG_WARNING, "Font file not found: %s", font_path);
        return false;
    }

    ctx.font_small  = LoadFontEx(font_path, 16, nullptr, 0);
    ctx.font_medium = LoadFontEx(font_path, 28, nullptr, 0);
    ctx.font_large  = LoadFontEx(font_path, 48, nullptr, 0);

    // Check if any font failed to load (baseSize will be 0)
    if (ctx.font_small.baseSize == 0 ||
        ctx.font_medium.baseSize == 0 ||
        ctx.font_large.baseSize == 0) {
        TraceLog(LOG_WARNING, "Failed to load font: %s", font_path);
        text_shutdown(ctx);
        return false;
    }

    // Enable bilinear filtering for smoother text at non-native sizes
    SetTextureFilter(ctx.font_small.texture,  TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(ctx.font_medium.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(ctx.font_large.texture,  TEXTURE_FILTER_BILINEAR);

    ctx.loaded = true;
    return true;
}

// ── text_shutdown ───────────────────────────────────────────
void text_shutdown(TextContext& ctx) {
    if (ctx.font_large.baseSize > 0)  UnloadFont(ctx.font_large);
    if (ctx.font_medium.baseSize > 0) UnloadFont(ctx.font_medium);
    if (ctx.font_small.baseSize > 0)  UnloadFont(ctx.font_small);
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

    const Font& font = pick_font(ctx, font_size);
    float fontSize = static_cast<float>(font.baseSize);
    float spacing = 1.0f;

    Vector2 size = MeasureTextEx(font, text, fontSize, spacing);

    float draw_x = x;
    switch (align) {
        case TextAlign::Left:                            break;
        case TextAlign::Center: draw_x = x - size.x / 2; break;
        case TextAlign::Right:  draw_x = x - size.x;      break;
    }

    DrawTextEx(font, text, {draw_x, y}, fontSize, spacing,
               {r, g, b, a});
}

// ── text_draw_number ────────────────────────────────────────
void text_draw_number(const TextContext& ctx,
                      int number,
                      float x, float y,
                      FontSize font_size,
                      uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                      TextAlign align) {
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%d", number);
    text_draw(ctx, buf, x, y, font_size, r, g, b, a, align);
}

// ── text_width ──────────────────────────────────────────────
int text_width(const TextContext& ctx, const char* text, FontSize font_size) {
    if (!text || !ctx.loaded) return 0;
    const Font& font = pick_font(ctx, font_size);
    float fontSize = static_cast<float>(font.baseSize);
    Vector2 size = MeasureTextEx(font, text, fontSize, 1.0f);
    return static_cast<int>(size.x);
}
