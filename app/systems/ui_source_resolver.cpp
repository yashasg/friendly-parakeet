#include "ui_source_resolver.h"

#include <cstdio>

#include "../components/scoring.h"
#include "../components/song_state.h"
#include "../components/settings.h"

namespace {

const char* death_cause_to_string(DeathCause cause) {
    switch (cause) {
        case DeathCause::EnergyDepleted: return "ENERGY DEPLETED";
        case DeathCause::MissedABeat:    return "MISSED A BEAT";
        case DeathCause::HitABar:        return "HIT A BAR";
        case DeathCause::None:           break;
    }
    return "";
}

}  // namespace

std::optional<std::int32_t> resolve_ui_int_source(entt::registry& reg,
                                                   std::string_view source) {
    if (source == "ScoreState.score") {
        auto* score = reg.ctx().find<ScoreState>();
        if (!score) return std::nullopt;
        return score->score;
    }
    if (source == "ScoreState.displayed_score") {
        auto* score = reg.ctx().find<ScoreState>();
        if (!score) return std::nullopt;
        return score->displayed_score;
    }
    if (source == "ScoreState.high_score") {
        auto* score = reg.ctx().find<ScoreState>();
        if (!score) return std::nullopt;
        return score->high_score;
    }
    if (source == "ScoreState.chain_count") {
        auto* score = reg.ctx().find<ScoreState>();
        if (!score) return std::nullopt;
        return score->chain_count;
    }

    if (source == "SongResults.perfect_count") {
        auto* results = reg.ctx().find<SongResults>();
        if (!results) return std::nullopt;
        return static_cast<std::int32_t>(results->perfect_count);
    }
    if (source == "SongResults.good_count") {
        auto* results = reg.ctx().find<SongResults>();
        if (!results) return std::nullopt;
        return static_cast<std::int32_t>(results->good_count);
    }
    if (source == "SongResults.ok_count") {
        auto* results = reg.ctx().find<SongResults>();
        if (!results) return std::nullopt;
        return static_cast<std::int32_t>(results->ok_count);
    }
    if (source == "SongResults.bad_count") {
        auto* results = reg.ctx().find<SongResults>();
        if (!results) return std::nullopt;
        return static_cast<std::int32_t>(results->bad_count);
    }
    if (source == "SongResults.miss_count") {
        auto* results = reg.ctx().find<SongResults>();
        if (!results) return std::nullopt;
        return static_cast<std::int32_t>(results->miss_count);
    }
    if (source == "SongResults.max_chain") {
        auto* results = reg.ctx().find<SongResults>();
        if (!results) return std::nullopt;
        return static_cast<std::int32_t>(results->max_chain);
    }
    if (source == "SongResults.total_notes") {
        auto* results = reg.ctx().find<SongResults>();
        if (!results) return std::nullopt;
        return static_cast<std::int32_t>(results->total_notes);
    }

    if (source == "SettingsState.audio_offset_ms") {
        auto* settings = reg.ctx().find<SettingsState>();
        if (!settings) return std::nullopt;
        return static_cast<std::int32_t>(settings->audio_offset_ms);
    }
    if (source == "SettingsState.haptics_enabled") {
        auto* settings = reg.ctx().find<SettingsState>();
        if (!settings) return std::nullopt;
        return settings->haptics_enabled ? 1 : 0;
    }
    if (source == "SettingsState.reduce_motion") {
        auto* settings = reg.ctx().find<SettingsState>();
        if (!settings) return std::nullopt;
        return settings->reduce_motion ? 1 : 0;
    }

    return std::nullopt;
}

std::optional<std::string> resolve_ui_dynamic_text(entt::registry& reg,
                                                    std::string_view source,
                                                    std::string_view format) {
    // String-only source: cause-of-death readout.
    if (source == "GameOverState.reason") {
        auto* gos = reg.ctx().find<GameOverState>();
        if (!gos) return std::nullopt;
        return std::string(death_cause_to_string(gos->cause));
    }

    // Composite button-face labels for Settings toggles.
    if (format == "haptics_button") {
        auto v = resolve_ui_int_source(reg, source);
        if (!v.has_value()) return std::nullopt;
        return std::string(*v ? "HAPTICS: ON" : "HAPTICS: OFF");
    }
    if (format == "motion_button") {
        auto v = resolve_ui_int_source(reg, source);
        if (!v.has_value()) return std::nullopt;
        return std::string(*v ? "MOTION: ON" : "MOTION: OFF");
    }

    // Numeric sources with formatters.
    auto v = resolve_ui_int_source(reg, source);
    if (!v.has_value()) return std::nullopt;

    if (format == "on_off") {
        return std::string(*v ? "ON" : "OFF");
    }
    if (format == "signed_ms") {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%+d ms", *v);
        return std::string(buf);
    }

    // Default: plain decimal integer.
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", *v);
    return std::string(buf);
}
