// Regression tests for spawn_ui_elements() malformed/unimplemented-schema paths — issues #346, #347.
//
// Kujan's review of PR #338 noted that all three catch branches inside
// spawn_ui_elements() lacked test coverage:
//   1. text/text_dynamic animation catch  — entity survives without UIAnimation
//   2. button outer catch                 — entity is destroyed (no orphan)
//   3. shape outer catch                  — entity is destroyed (no orphan)
//   4. button inner animation catch       — entity survives with UIButton only
//
// Tests use an in-memory nlohmann::json screen document, so no on-disk
// content files are required and results are fully deterministic.

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <entt/entt.hpp>
#include "ui/ui_loader.h"
#include "components/ui_element.h"
#include "components/transform.h"
#include "components/rendering.h"

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace {

json make_screen(json elements) {
    return json{{"elements", std::move(elements)}};
}

json text_el(const std::string& id, const std::string& text = "HELLO") {
    return json{{"id", id}, {"type", "text"}, {"text", text}};
}

json button_el(const std::string& id) {
    return json{
        {"id", id}, {"type", "button"}, {"text", "OK"},
        {"w_n", 0.4f}, {"h_n", 0.1f},
        {"bg_color",     json::array({0, 0, 0, 255})},
        {"border_color", json::array({255, 255, 255, 255})},
        {"text_color",   json::array({255, 255, 255, 255})}
    };
}

json shape_el(const std::string& id) {
    return json{
        {"id", id}, {"type", "shape"}, {"shape", "circle"},
        {"size_n", 0.05f},
        {"color", json::array({255, 0, 0, 255})}
    };
}

json bad_animation() {
    // "speed" key is intentionally absent — nlohmann::at() will throw.
    return json{{"speed_typo", 1.0f}, {"alpha_range", json::array({0, 255})}};
}

} // namespace

// ---------------------------------------------------------------------------
// Baseline — no elements key
// ---------------------------------------------------------------------------

TEST_CASE("spawn_ui_elements: screen with no elements key creates no entities", "[ui][spawn][issue346]") {
    entt::registry reg;
    json screen = json::object();
    spawn_ui_elements(reg, screen);
    CHECK(reg.view<UIElementTag>().size() == 0);
}

TEST_CASE("spawn_ui_elements: screen with empty elements array creates no entities", "[ui][spawn][issue346]") {
    entt::registry reg;
    spawn_ui_elements(reg, make_screen(json::array()));
    CHECK(reg.view<UIElementTag>().size() == 0);
}

TEST_CASE("spawn_ui_elements: renderable elements declare HUD pass and UI position", "[ui][spawn][render]") {
    entt::registry reg;
    json els = json::array({
        text_el("txt"),
        button_el("btn"),
        shape_el("shape")
    });

    spawn_ui_elements(reg, make_screen(els));

    int count = 0;
    auto view = reg.view<UIElementTag, UIPosition, TagHUDPass>();
    for (auto entity : view) {
        CHECK_FALSE(reg.all_of<Position>(entity));
        ++count;
    }
    CHECK(count == 3);
}

// ---------------------------------------------------------------------------
// Text element — animation catch branch
// ---------------------------------------------------------------------------

TEST_CASE("spawn_ui_elements: text with missing animation.speed does not abort", "[ui][spawn][issue346]") {
    entt::registry reg;
    json el = text_el("lbl");
    el["animation"] = bad_animation();  // missing "speed"
    spawn_ui_elements(reg, make_screen(json::array({el})));

    // Entity must survive — it is renderable (has UIText).
    REQUIRE(reg.view<UIElementTag>().size() == 1);
    auto entity = *reg.view<UIElementTag>().begin();
    CHECK(reg.all_of<UIText>(entity));
    // UIAnimation must NOT be attached — the catch swallowed the partial build.
    CHECK_FALSE(reg.all_of<UIAnimation>(entity));
}

TEST_CASE("spawn_ui_elements: text with malformed alpha_range does not abort", "[ui][spawn][issue346]") {
    entt::registry reg;
    json el = text_el("lbl2");
    el["animation"] = json{{"speed", 1.0f}, {"alpha_range", "bad_string"}};  // wrong type
    spawn_ui_elements(reg, make_screen(json::array({el})));

    REQUIRE(reg.view<UIElementTag>().size() == 1);
    auto entity = *reg.view<UIElementTag>().begin();
    CHECK(reg.all_of<UIText>(entity));
    CHECK_FALSE(reg.all_of<UIAnimation>(entity));
}

TEST_CASE("spawn_ui_elements: text with valid animation attaches UIAnimation", "[ui][spawn][issue346]") {
    entt::registry reg;
    json el = text_el("lbl3");
    el["animation"] = json{{"speed", 2.0f}, {"alpha_range", json::array({50, 200})}};
    spawn_ui_elements(reg, make_screen(json::array({el})));

    REQUIRE(reg.view<UIElementTag>().size() == 1);
    auto entity = *reg.view<UIElementTag>().begin();
    CHECK(reg.all_of<UIText, UIAnimation>(entity));
    const auto& anim = reg.get<UIAnimation>(entity);
    CHECK(anim.speed == 2.0f);
    CHECK(anim.alpha_min == 50);
    CHECK(anim.alpha_max == 200);
}

// ---------------------------------------------------------------------------
// Button outer catch — missing required color fields
// ---------------------------------------------------------------------------

TEST_CASE("spawn_ui_elements: button missing bg_color destroys entity (no orphan)", "[ui][spawn][issue346]") {
    entt::registry reg;
    json el = json{
        {"id", "btn_bad"}, {"type", "button"}, {"text", "X"},
        {"w_n", 0.4f}, {"h_n", 0.1f}
        // bg_color, border_color, text_color intentionally absent
    };
    spawn_ui_elements(reg, make_screen(json::array({el})));

    CHECK(reg.view<UIElementTag>().size() == 0);
}

TEST_CASE("spawn_ui_elements: button missing border_color destroys entity (no orphan)", "[ui][spawn][issue346]") {
    entt::registry reg;
    json el = json{
        {"id", "btn_bad2"}, {"type", "button"}, {"text", "X"},
        {"w_n", 0.4f}, {"h_n", 0.1f},
        {"bg_color", json::array({0, 0, 0, 255})}
        // border_color absent
    };
    spawn_ui_elements(reg, make_screen(json::array({el})));

    CHECK(reg.view<UIElementTag>().size() == 0);
}

// ---------------------------------------------------------------------------
// Button inner animation catch — button entity survives
// ---------------------------------------------------------------------------

TEST_CASE("spawn_ui_elements: button with bad animation still spawns UIButton", "[ui][spawn][issue346]") {
    entt::registry reg;
    json el = button_el("btn_anim");
    el["animation"] = bad_animation();
    spawn_ui_elements(reg, make_screen(json::array({el})));

    REQUIRE(reg.view<UIElementTag>().size() == 1);
    auto entity = *reg.view<UIElementTag>().begin();
    CHECK(reg.all_of<UIButton>(entity));
    CHECK_FALSE(reg.all_of<UIAnimation>(entity));
}

// ---------------------------------------------------------------------------
// Shape outer catch — missing color field
// ---------------------------------------------------------------------------

TEST_CASE("spawn_ui_elements: shape missing color destroys entity (no orphan)", "[ui][spawn][issue346]") {
    entt::registry reg;
    json el = json{
        {"id", "sh_bad"}, {"type", "shape"}, {"shape", "circle"}, {"size_n", 0.1f}
        // color absent
    };
    spawn_ui_elements(reg, make_screen(json::array({el})));

    CHECK(reg.view<UIElementTag>().size() == 0);
}

TEST_CASE("spawn_ui_elements: shape with valid fields spawns UIShape", "[ui][spawn][issue346]") {
    entt::registry reg;
    spawn_ui_elements(reg, make_screen(json::array({shape_el("sh_ok")})));

    REQUIRE(reg.view<UIElementTag>().size() == 1);
    auto entity = *reg.view<UIElementTag>().begin();
    CHECK(reg.all_of<UIShape>(entity));
}

// ---------------------------------------------------------------------------
// Mixed: valid + invalid elements — only valid entities survive
// ---------------------------------------------------------------------------

TEST_CASE("spawn_ui_elements: mix of good text and bad button leaves only text entity", "[ui][spawn][issue346]") {
    entt::registry reg;
    json bad_btn = json{
        {"id", "bad_btn"}, {"type", "button"}, {"text", "X"}
        // missing all color fields
    };
    json good_text = text_el("ok_lbl", "SCORE");

    spawn_ui_elements(reg, make_screen(json::array({bad_btn, good_text})));

    REQUIRE(reg.view<UIElementTag>().size() == 1);
    auto entity = *reg.view<UIElementTag>().begin();
    CHECK(reg.all_of<UIText>(entity));
    CHECK_FALSE(reg.all_of<UIButton>(entity));
}

TEST_CASE("spawn_ui_elements: mix of good button and bad shape leaves only button entity", "[ui][spawn][issue346]") {
    entt::registry reg;
    json good_btn = button_el("ok_btn");
    json bad_sh = json{
        {"id", "bad_sh"}, {"type", "shape"}, {"shape", "square"}, {"size_n", 0.1f}
        // color absent
    };

    spawn_ui_elements(reg, make_screen(json::array({good_btn, bad_sh})));

    REQUIRE(reg.view<UIElementTag>().size() == 1);
    auto entity = *reg.view<UIElementTag>().begin();
    CHECK(reg.all_of<UIButton>(entity));
    CHECK_FALSE(reg.all_of<UIShape>(entity));
}

// ---------------------------------------------------------------------------
// #347 — Supported-but-unbranched types must not leave orphan entities
// ---------------------------------------------------------------------------

TEST_CASE("spawn_ui_elements: stat_table does not leave orphan entity", "[ui][spawn][issue347]") {
    entt::registry reg;
    json el = json{{"id", "my_table"}, {"type", "stat_table"}};
    spawn_ui_elements(reg, make_screen(json::array({el})));
    CHECK(reg.view<UIElementTag>().size() == 0);
}

TEST_CASE("spawn_ui_elements: card_list does not leave orphan entity", "[ui][spawn][issue347]") {
    entt::registry reg;
    json el = json{{"id", "my_cards"}, {"type", "card_list"}};
    spawn_ui_elements(reg, make_screen(json::array({el})));
    CHECK(reg.view<UIElementTag>().size() == 0);
}

TEST_CASE("spawn_ui_elements: line does not leave orphan entity", "[ui][spawn][issue347]") {
    entt::registry reg;
    json el = json{{"id", "my_line"}, {"type", "line"}};
    spawn_ui_elements(reg, make_screen(json::array({el})));
    CHECK(reg.view<UIElementTag>().size() == 0);
}

TEST_CASE("spawn_ui_elements: burnout_meter does not leave orphan entity", "[ui][spawn][issue347]") {
    entt::registry reg;
    json el = json{{"id", "my_meter"}, {"type", "burnout_meter"}};
    spawn_ui_elements(reg, make_screen(json::array({el})));
    CHECK(reg.view<UIElementTag>().size() == 0);
}

TEST_CASE("spawn_ui_elements: shape_button_row does not leave orphan entity", "[ui][spawn][issue347]") {
    entt::registry reg;
    json el = json{{"id", "my_row"}, {"type", "shape_button_row"}};
    spawn_ui_elements(reg, make_screen(json::array({el})));
    CHECK(reg.view<UIElementTag>().size() == 0);
}

TEST_CASE("spawn_ui_elements: button_row does not leave orphan entity", "[ui][spawn][issue347]") {
    entt::registry reg;
    json el = json{{"id", "my_btn_row"}, {"type", "button_row"}};
    spawn_ui_elements(reg, make_screen(json::array({el})));
    CHECK(reg.view<UIElementTag>().size() == 0);
}

TEST_CASE("spawn_ui_elements: unknown type does not leave orphan entity", "[ui][spawn][issue347]") {
    entt::registry reg;
    json el = json{{"id", "mystery"}, {"type", "totally_unknown_type"}};
    spawn_ui_elements(reg, make_screen(json::array({el})));
    CHECK(reg.view<UIElementTag>().size() == 0);
}

TEST_CASE("spawn_ui_elements: mix of valid text and unbranched types leaves only text", "[ui][spawn][issue347]") {
    entt::registry reg;
    json els = json::array({
        text_el("lbl_ok", "HELLO"),
        json{{"id", "tbl"}, {"type", "stat_table"}},
        json{{"id", "meter"}, {"type", "burnout_meter"}},
        json{{"id", "ln"}, {"type", "line"}}
    });
    spawn_ui_elements(reg, make_screen(els));

    REQUIRE(reg.view<UIElementTag>().size() == 1);
    auto entity = *reg.view<UIElementTag>().begin();
    CHECK(reg.all_of<UIText>(entity));
}
