// Regression test for #513: the Tutorial "DODGE LANES" hint must vary with
// the runtime input-capability flag, not the compile-time PLATFORM_HAS_KEYBOARD
// macro. The previous compile-time #ifdef (since replaced by the runtime
// switch in app/util/tutorial_dodge_hint.h) shipped keyboard-only copy to
// touch-only Web browsers because the Web build defines PLATFORM_HAS_KEYBOARD
// (CMakeLists.txt:198).

#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include <entt/entt.hpp>
#include "util/tutorial_dodge_hint.h"
#include "systems/web_input_policy.h"

TEST_CASE("tutorial dodge hint copy switches on touch capability (#513)",
          "[ui][tutorial][web]") {
    SECTION("touch-capable browsers see the swipe copy") {
        const char* hint = tutorial_dodge_hint_text(/*prefer_touch=*/true);
        REQUIRE(hint != nullptr);
        REQUIRE(std::strcmp(hint, "SWIPE LEFT OR RIGHT") == 0);
    }

    SECTION("keyboard-capable contexts see the keyboard copy") {
        const char* hint = tutorial_dodge_hint_text(/*prefer_touch=*/false);
        REQUIRE(hint != nullptr);
        REQUIRE(std::strcmp(hint, "DODGE: A/D OR ARROWS") == 0);
    }

    SECTION("hint bounds match the (110, 710, 500, 32) RGL rectangle") {
        Rectangle r = tutorial_dodge_hint_bounds({0.0f, 0.0f});
        REQUIRE(r.x == 110.0f);
        REQUIRE(r.y == 710.0f);
        REQUIRE(r.width  == 500.0f);
        REQUIRE(r.height == 32.0f);
    }

    SECTION("hint bounds track the layout anchor") {
        Rectangle r = tutorial_dodge_hint_bounds({25.0f, 50.0f});
        REQUIRE(r.x == 135.0f);
        REQUIRE(r.y == 760.0f);
    }
}

TEST_CASE("web_input_touch_capable defaults to false on native builds (#513)",
          "[ui][tutorial][web_policy]") {
    entt::registry reg;
    // No WebInputPolicy installed → accessor must return false, not crash.
    REQUIRE_FALSE(web_input_touch_capable(reg));
}
