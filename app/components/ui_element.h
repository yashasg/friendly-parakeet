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

// Renderable overlay (full-screen tinted rect).
struct UIOverlay {
    Color color = {0, 0, 0, 128};
};

// Optional: pulsing alpha animation.
struct UIAnimation {
    float   speed = 1.0f;
    uint8_t alpha_min = 0;
    uint8_t alpha_max = 255;
};
