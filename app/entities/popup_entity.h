#pragma once

struct Color;
struct PopupDisplay;
struct ScorePopup;

// One-time initializer for a PopupDisplay (text + font size + base color).
// Called at spawn so popup_display_system never re-formats static fields per
// frame.
void init_popup_display(PopupDisplay& pd,
                        const ScorePopup& sp,
                        const Color& base);
