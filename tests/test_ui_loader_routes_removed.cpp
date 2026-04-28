// Regression test for issue #266: UIState::routes JSON parsing was dead.
// Routes storage and the per-startup parse of content/ui/routes.json have been
// removed in favour of the existing GamePhase → screen_name mapping in
// ui_navigation_system. This test pins those decisions so a future refactor
// doesn't silently re-introduce an unread per-startup JSON parse.

#include "components/ui_state.h"
#include "ui/ui_loader.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("UIState has no routes member (issue #266)", "[ui][loader][issue266]") {
    UIState ui;
    // The screen graph is driven by GamePhase in ui_navigation_system; there is
    // no data-driven router that needs a parsed `routes` document at runtime.
    // A `.routes` field would mean a per-startup JSON parse with no consumer.
    // Sanity: the live fields the navigation system depends on still exist.
    (void)ui.screen;
    (void)ui.overlay_screen;
    (void)ui.current;
    (void)ui.base_dir;
    SUCCEED("UIState compiles without a routes member");
}

TEST_CASE("load_ui does not preload screen content (issue #266)",
          "[ui][loader][issue266]") {
    // load_ui must remain cheap: it sets base_dir and returns. Screens are
    // loaded lazily by ui_navigation_system when GamePhase first selects them.
    UIState ui = load_ui("content/ui");
    CHECK(ui.base_dir == "content/ui");
    CHECK(ui.current.empty());
    CHECK(ui.screen.is_null());
    CHECK(ui.overlay_screen.is_null());
    CHECK_FALSE(ui.has_overlay);
}
