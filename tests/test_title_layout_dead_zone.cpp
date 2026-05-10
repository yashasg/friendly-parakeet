// Regression test for #466: title-screen tap-to-Start dead-zones must be
// derived from the generated title_layout.h bounds accessors instead of
// duplicated literals in title_screen_controller.cpp.
//
// We assert two invariants:
//   1. The Settings/Exit bounds accessors return non-degenerate rectangles
//      that lie within the canonical 720x1280 layout viewport. If the .rgl
//      source moves the buttons, regenerating the header updates these
//      accessors and the controller picks up the new dead-zones for free.
//   2. Settings and Exit bounds do not overlap each other (sanity for the
//      "tap anywhere except Settings/Exit" semantics).

#include <catch2/catch_test_macros.hpp>

#include <raylib.h>
#include <raygui.h>
#include "ui/generated/title_layout.h"

namespace {

bool rects_overlap(Rectangle a, Rectangle b) {
    return !(a.x + a.width  <= b.x ||
             b.x + b.width  <= a.x ||
             a.y + a.height <= b.y ||
             b.y + b.height <= a.y);
}

bool rect_within_viewport(Rectangle r, float w, float h) {
    return r.x >= 0.0f && r.y >= 0.0f &&
           r.width > 0.0f && r.height > 0.0f &&
           r.x + r.width  <= w &&
           r.y + r.height <= h;
}

} // namespace

TEST_CASE("title layout exposes bounds accessors for dead-zone reuse (#466)", "[ui][title]") {
    TitleLayoutState state = TitleLayout_Init();

    const Rectangle settings = TitleLayout_SettingsButtonBounds(&state);
    const Rectangle exit_btn = TitleLayout_ExitButtonBounds(&state);

    SECTION("bounds are non-degenerate and inside the 720x1280 layout viewport") {
        REQUIRE(rect_within_viewport(settings, TITLE_LAYOUT_WIDTH, TITLE_LAYOUT_HEIGHT));
        REQUIRE(rect_within_viewport(exit_btn, TITLE_LAYOUT_WIDTH, TITLE_LAYOUT_HEIGHT));
    }

    SECTION("Settings and Exit bounds do not overlap") {
        REQUIRE_FALSE(rects_overlap(settings, exit_btn));
    }

    SECTION("bounds track Anchor01 so dead-zones move with the layout") {
        state.Anchor01 = {25.0f, 50.0f};
        const Rectangle settings_moved = TitleLayout_SettingsButtonBounds(&state);
        REQUIRE(settings_moved.x == settings.x + 25.0f);
        REQUIRE(settings_moved.y == settings.y + 50.0f);
        REQUIRE(settings_moved.width  == settings.width);
        REQUIRE(settings_moved.height == settings.height);
    }

    SECTION("center of Settings/Exit rect collides with itself (dead-zone sanity)") {
        const Vector2 settings_center = {settings.x + settings.width  / 2.0f,
                                          settings.y + settings.height / 2.0f};
        const Vector2 exit_center     = {exit_btn.x + exit_btn.width  / 2.0f,
                                          exit_btn.y + exit_btn.height / 2.0f};
        REQUIRE(CheckCollisionPointRec(settings_center, settings));
        REQUIRE(CheckCollisionPointRec(exit_center, exit_btn));
        // A point clearly above both buttons should not collide with either —
        // this is the pixel range where "tap to Start" is intended to fire.
        const Vector2 above = {360.0f, 700.0f};
        REQUIRE_FALSE(CheckCollisionPointRec(above, settings));
        REQUIRE_FALSE(CheckCollisionPointRec(above, exit_btn));
    }
}
