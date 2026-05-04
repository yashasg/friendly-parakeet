#pragma once

#include "platform/runtime_api.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DEFAULT = 0,
    LABEL = 1,
    BUTTON = 2,
};

enum {
    BORDER_COLOR_NORMAL = 0,
    BASE_COLOR_NORMAL,
    TEXT_COLOR_NORMAL,
    BORDER_COLOR_FOCUSED,
    BASE_COLOR_FOCUSED,
    TEXT_COLOR_FOCUSED,
    BORDER_COLOR_PRESSED,
    BASE_COLOR_PRESSED,
    TEXT_COLOR_PRESSED,
    BORDER_COLOR_DISABLED,
    BASE_COLOR_DISABLED,
    TEXT_COLOR_DISABLED,
    BORDER_WIDTH,
    TEXT_PADDING,
    TEXT_ALIGNMENT,
    RESERVED,
    TEXT_SIZE,
};

enum {
    TEXT_ALIGN_LEFT = 0,
    TEXT_ALIGN_CENTER = 1,
    TEXT_ALIGN_RIGHT = 2,
};

int GuiGetStyle(int control, int property);
void GuiSetStyle(int control, int property, int value);
void GuiSetAlpha(float alpha);
int GuiLabel(Rectangle bounds, const char* text);
int GuiButton(Rectangle bounds, const char* text);

#ifdef __cplusplus
}
#endif
