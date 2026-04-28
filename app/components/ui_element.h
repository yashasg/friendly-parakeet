#pragma once
#include <raylib.h>
#include "../components/text.h"
#include "../components/player.h"
#include <cstdint>

// UI element types that the renderer knows how to draw.
enum class UIElementType : uint8_t { Text, Button, Shape, Overlay };

// Tag: entity is a UI element, destroyed on screen change.
struct UIElementTag {};

// Renderable UI text element.
struct UIText {
    char     text[64] = {};
    FontSize font_size = FontSize::Medium;
    TextAlign align = TextAlign::Left;
    Color    color = WHITE;
};

// Renderable UI button element.
struct UIButton {
    char     text[64] = {};
    FontSize font_size = FontSize::Small;
    float    w = 0, h = 0;
    float    corner_radius = 0.2f;
    Color    bg = BLACK;
    Color    border = WHITE;
    Color    text_color = WHITE;
};

// Renderable UI shape element.
struct UIShape {
    Shape shape = Shape::Circle;
    float size  = 0;
    Color color = WHITE;
};

// Optional: dynamic text source. Attached alongside UIText or UIButton
// to override the static `text` field at render time by resolving
// `source` (and applying `format`) through ui_source_resolver.
struct UIDynamicText {
    char source[64] = {};
    char format[24] = {};
};

// Optional: pulsing alpha animation.
struct UIAnimation {
    float   speed = 1.0f;
    uint8_t alpha_min = 0;
    uint8_t alpha_max = 255;
};
