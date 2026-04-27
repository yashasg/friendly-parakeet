#include <catch2/catch_test_macros.hpp>
#include <entt/entity/registry.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

#include "components/scoring.h"
#include "components/song_state.h"
#include "components/settings.h"
#include "systems/ui_source_resolver.h"

TEST_CASE("UI source resolver: resolves ScoreState integer sources", "[ui]") {
    entt::registry reg;
    auto& score = reg.ctx().emplace<ScoreState>();
    score.score = 12345;
    score.displayed_score = 12000;
    score.high_score = 54321;
    score.chain_count = 17;

    CHECK(resolve_ui_int_source(reg, "ScoreState.score").value() == 12345);
    CHECK(resolve_ui_int_source(reg, "ScoreState.displayed_score").value() == 12000);
    CHECK(resolve_ui_int_source(reg, "ScoreState.high_score").value() == 54321);
    CHECK(resolve_ui_int_source(reg, "ScoreState.chain_count").value() == 17);
}

TEST_CASE("UI source resolver: resolves SongResults integer sources", "[ui]") {
    entt::registry reg;
    auto& results = reg.ctx().emplace<SongResults>();
    results.perfect_count = 11;
    results.good_count = 7;
    results.ok_count = 5;
    results.bad_count = 3;
    results.miss_count = 2;
    results.max_chain = 19;
    results.total_notes = 28;

    CHECK(resolve_ui_int_source(reg, "SongResults.perfect_count").value() == 11);
    CHECK(resolve_ui_int_source(reg, "SongResults.good_count").value() == 7);
    CHECK(resolve_ui_int_source(reg, "SongResults.ok_count").value() == 5);
    CHECK(resolve_ui_int_source(reg, "SongResults.bad_count").value() == 3);
    CHECK(resolve_ui_int_source(reg, "SongResults.miss_count").value() == 2);
    CHECK(resolve_ui_int_source(reg, "SongResults.max_chain").value() == 19);
    CHECK(resolve_ui_int_source(reg, "SongResults.total_notes").value() == 28);
}

TEST_CASE("UI source resolver: rejects unknown or unavailable sources", "[ui]") {
    entt::registry reg;

    CHECK_FALSE(resolve_ui_int_source(reg, "ScoreState.score").has_value());
    CHECK_FALSE(resolve_ui_int_source(reg, "SongResults.perfect_count").has_value());

    reg.ctx().emplace<ScoreState>();
    reg.ctx().emplace<SongResults>();

    CHECK_FALSE(resolve_ui_int_source(reg, "").has_value());
    CHECK_FALSE(resolve_ui_int_source(reg, "ScoreState").has_value());
    CHECK_FALSE(resolve_ui_int_source(reg, "ScoreState.unknown").has_value());
    CHECK_FALSE(resolve_ui_int_source(reg, "Unknown.value").has_value());
}

TEST_CASE("UI source resolver: resolves SettingsState sources", "[ui]") {
    entt::registry reg;
    auto& settings = reg.ctx().emplace<SettingsState>();
    settings.audio_offset_ms = -30;
    settings.haptics_enabled = false;
    settings.reduce_motion = true;

    CHECK(resolve_ui_int_source(reg, "SettingsState.audio_offset_ms").value() == -30);
    CHECK(resolve_ui_int_source(reg, "SettingsState.haptics_enabled").value() == 0);
    CHECK(resolve_ui_int_source(reg, "SettingsState.reduce_motion").value() == 1);
}

TEST_CASE("UI source resolver: all shipped Song Complete sources resolve", "[ui]") {
    entt::registry reg;
    reg.ctx().emplace<ScoreState>();
    reg.ctx().emplace<SongResults>();

    std::ifstream file("content/ui/screens/song_complete.json");
    REQUIRE(file.is_open());
    auto screen = nlohmann::json::parse(file);

    int resolved_sources = 0;
    for (const auto& el : screen["elements"]) {
        const auto type = el.value("type", "");
        if (type == "text_dynamic") {
            auto value = resolve_ui_int_source(reg, el.value("source", ""));
            CHECK(value.has_value());
            ++resolved_sources;
        } else if (type == "stat_table") {
            for (const auto& row : el["rows"]) {
                auto value = resolve_ui_int_source(reg, row.value("source", ""));
                CHECK(value.has_value());
                ++resolved_sources;
            }
        }
    }

    CHECK(resolved_sources == 7);
}
