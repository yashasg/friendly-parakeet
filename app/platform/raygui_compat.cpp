#include "../raygui.h"

#include <array>

namespace {

constexpr int kControlCount = 8;
constexpr int kPropertyCount = 24;

std::array<std::array<int, kPropertyCount>, kControlCount>& style_table() {
    static std::array<std::array<int, kPropertyCount>, kControlCount> table{};
    static bool initialized = false;
    if (!initialized) {
        for (auto& control : table) {
            control.fill(0);
            control[TEXT_SIZE] = 20;
            control[TEXT_ALIGNMENT] = TEXT_ALIGN_LEFT;
        }
        initialized = true;
    }
    return table;
}

float& gui_alpha() {
    static float alpha = 1.0f;
    return alpha;
}

Color alpha_tinted(Color c) {
    const float a = Clamp(gui_alpha(), 0.0f, 1.0f);
    c.a = static_cast<unsigned char>(Clamp(static_cast<float>(c.a) * a, 0.0f, 255.0f));
    return c;
}

}  // namespace

extern "C" int GuiGetStyle(int control, int property) {
    if (control < 0 || control >= kControlCount || property < 0 || property >= kPropertyCount) return 0;
    return style_table()[control][property];
}

extern "C" void GuiSetStyle(int control, int property, int value) {
    if (control < 0 || control >= kControlCount || property < 0 || property >= kPropertyCount) return;
    style_table()[control][property] = value;
}

extern "C" void GuiSetAlpha(float alpha) {
    gui_alpha() = alpha;
}

extern "C" int GuiLabel(Rectangle bounds, const char* text) {
    const int text_size = GuiGetStyle(DEFAULT, TEXT_SIZE);
    const int alignment = GuiGetStyle(LABEL, TEXT_ALIGNMENT);
    float x = bounds.x + 4.0f;
    if (alignment == TEXT_ALIGN_CENTER) x = bounds.x + bounds.width * 0.5f;
    if (alignment == TEXT_ALIGN_RIGHT) x = bounds.x + bounds.width - 4.0f;
    DrawTextEx(GetFontDefault(), text ? text : "", {x, bounds.y + 4.0f}, static_cast<float>(text_size), 1.0f,
               alpha_tinted(WHITE));
    return 0;
}

extern "C" int GuiButton(Rectangle bounds, const char* text) {
    const Vector2 mouse = GetMousePosition();
    const bool hover = CheckCollisionPointRec(mouse, bounds);

    Color fill = {40, 40, 60, 255};
    Color border = {80, 100, 140, 255};
    if (hover) {
        fill = {60, 70, 100, 255};
        border = {120, 160, 230, 255};
    }

    DrawRectangleRec(bounds, alpha_tinted(fill));
    DrawRectangleLinesEx(bounds, 1.5f, alpha_tinted(border));

    const int text_size = GuiGetStyle(DEFAULT, TEXT_SIZE);
    DrawTextEx(GetFontDefault(), text ? text : "", {bounds.x + 10.0f, bounds.y + (bounds.height - text_size) * 0.5f},
               static_cast<float>(text_size), 1.0f, alpha_tinted(WHITE));

    return (hover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) ? 1 : 0;
}
