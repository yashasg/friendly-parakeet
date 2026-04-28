#include "ui_source_resolver.h"

#include <cstdio>
#include <entt/entt.hpp>

#include "../components/scoring.h"
#include "../components/song_state.h"
#include "../util/settings.h"

using namespace entt::literals;

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

std::optional<std::int32_t> resolve_ui_int_source(const entt::registry& reg,
                                                   std::string_view source) {
    // Sources come from JSON-parsed std::string fields; data() is null-terminated.
    switch (entt::hashed_string::value(source.data())) {  // #310 hashed dispatch

        case "ScoreState.score"_hs: {
            auto* score = reg.ctx().find<ScoreState>();
            if (!score) return std::nullopt;
            return score->score;
        }
        case "ScoreState.displayed_score"_hs: {
            auto* score = reg.ctx().find<ScoreState>();
            if (!score) return std::nullopt;
            return score->displayed_score;
        }
        case "ScoreState.high_score"_hs: {
            auto* score = reg.ctx().find<ScoreState>();
            if (!score) return std::nullopt;
            return score->high_score;
        }
        case "ScoreState.chain_count"_hs: {
            auto* score = reg.ctx().find<ScoreState>();
            if (!score) return std::nullopt;
            return score->chain_count;
        }

        case "SongResults.perfect_count"_hs: {
            auto* results = reg.ctx().find<SongResults>();
            if (!results) return std::nullopt;
            return static_cast<std::int32_t>(results->perfect_count);
        }
        case "SongResults.good_count"_hs: {
            auto* results = reg.ctx().find<SongResults>();
            if (!results) return std::nullopt;
            return static_cast<std::int32_t>(results->good_count);
        }
        case "SongResults.ok_count"_hs: {
            auto* results = reg.ctx().find<SongResults>();
            if (!results) return std::nullopt;
            return static_cast<std::int32_t>(results->ok_count);
        }
        case "SongResults.bad_count"_hs: {
            auto* results = reg.ctx().find<SongResults>();
            if (!results) return std::nullopt;
            return static_cast<std::int32_t>(results->bad_count);
        }
        case "SongResults.miss_count"_hs: {
            auto* results = reg.ctx().find<SongResults>();
            if (!results) return std::nullopt;
            return static_cast<std::int32_t>(results->miss_count);
        }
        case "SongResults.max_chain"_hs: {
            auto* results = reg.ctx().find<SongResults>();
            if (!results) return std::nullopt;
            return static_cast<std::int32_t>(results->max_chain);
        }
        case "SongResults.total_notes"_hs: {
            auto* results = reg.ctx().find<SongResults>();
            if (!results) return std::nullopt;
            return static_cast<std::int32_t>(results->total_notes);
        }

        case "SettingsState.audio_offset_ms"_hs: {
            auto* settings = reg.ctx().find<SettingsState>();
            if (!settings) return std::nullopt;
            return static_cast<std::int32_t>(settings->audio_offset_ms);
        }
        case "SettingsState.haptics_enabled"_hs: {
            auto* settings = reg.ctx().find<SettingsState>();
            if (!settings) return std::nullopt;
            return settings->haptics_enabled ? 1 : 0;
        }
        case "SettingsState.reduce_motion"_hs: {
            auto* settings = reg.ctx().find<SettingsState>();
            if (!settings) return std::nullopt;
            return settings->reduce_motion ? 1 : 0;
        }

        default: return std::nullopt;
    }
}

std::optional<std::string> resolve_ui_dynamic_text(const entt::registry& reg,
                                                    std::string_view source,
                                                    std::string_view format) {
    // Sources/formats come from JSON-parsed std::string fields; data() is null-terminated.
    switch (entt::hashed_string::value(source.data())) {  // #310 hashed dispatch
        case "GameOverState.reason"_hs: {
            auto* gos = reg.ctx().find<GameOverState>();
            if (!gos) return std::nullopt;
            return std::string(death_cause_to_string(gos->cause));
        }
        default: break;
    }

    // Composite button-face labels for Settings toggles (format-driven).
    switch (entt::hashed_string::value(format.data())) {  // #310 hashed dispatch
        case "haptics_button"_hs: {
            auto v = resolve_ui_int_source(reg, source);
            if (!v.has_value()) return std::nullopt;
            return std::string(*v ? "HAPTICS: ON" : "HAPTICS: OFF");
        }
        case "motion_button"_hs: {
            auto v = resolve_ui_int_source(reg, source);
            if (!v.has_value()) return std::nullopt;
            return std::string(*v ? "MOTION: ON" : "MOTION: OFF");
        }
        default: break;
    }

    // Numeric sources with formatters.
    auto v = resolve_ui_int_source(reg, source);
    if (!v.has_value()) return std::nullopt;

    switch (entt::hashed_string::value(format.data())) {  // #310 hashed dispatch
        case "on_off"_hs: {
            return std::string(*v ? "ON" : "OFF");
        }
        case "signed_ms"_hs: {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%+d ms", *v);
            return std::string(buf);
        }
        default: break;
    }

    // Default: plain decimal integer.
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", *v);
    return std::string(buf);
}
