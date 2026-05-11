#include "beat_map_loader.h"
#include "rhythm_math.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <algorithm>
#include <optional>
#include <raylib.h>

using json = nlohmann::json;
namespace {
constexpr uint8_t kLaneBitsMask = 0x07;
constexpr const char* kValidationConstantsPath = "content/constants.json";

std::string constants_path_for_app_dir(const std::string& app_dir) {
    if (app_dir.empty()) {
        return kValidationConstantsPath;
    }
    const char last = app_dir.back();
    if (last == '/' || last == '\\') {
        return app_dir + kValidationConstantsPath;
    }
    return app_dir + "/" + kValidationConstantsPath;
}

bool has_only_lane_bits(const uint8_t mask) {
    return (mask & static_cast<uint8_t>(~kLaneBitsMask)) == 0;
}

int lane_bit_count(const uint8_t mask) {
    int count = 0;
    for (int lane = 0; lane < 3; ++lane) {
        if (((mask >> lane) & 1U) != 0U) {
            ++count;
        }
    }
    return count;
}

bool kind_requires_shape(const ObstacleKind kind) {
    return kind == ObstacleKind::ShapeGate ||
           kind == ObstacleKind::ComboGate ||
           kind == ObstacleKind::SplitPath;
}
} // namespace

static bool try_load_constants_from(const std::string& path, ValidationConstants& vc) {
    char* file_text = LoadFileText(path.c_str());
    if (file_text == nullptr) return false;

    try {
        const std::string content(file_text);
        UnloadFileText(file_text);
        file_text = nullptr;
        json j = json::parse(content);
        if (j.contains("validation")) {
            const auto& v = j["validation"];
            vc.bpm_min              = v.value("bpm_min",              vc.bpm_min);
            vc.bpm_max              = v.value("bpm_max",              vc.bpm_max);
            vc.offset_min           = v.value("offset_min",           vc.offset_min);
            vc.offset_max           = v.value("offset_max",           vc.offset_max);
            vc.lead_beats_min       = v.value("lead_beats_min",       vc.lead_beats_min);
            vc.lead_beats_max       = v.value("lead_beats_max",       vc.lead_beats_max);
            vc.min_shape_change_gap = v.value("min_shape_change_gap", vc.min_shape_change_gap);
        }
    } catch (const json::exception& e) {
        TraceLog(LOG_WARNING, "Failed to parse constants file '%s': %s", path.c_str(), e.what());
        if (file_text != nullptr) {
            UnloadFileText(file_text);
        }
        return false;
    }

    return true;
}

ValidationConstants load_validation_constants(const std::string& app_dir) {
    ValidationConstants vc;
    const std::string exe_relative_path =
        constants_path_for_app_dir(app_dir.empty() ? std::string(GetApplicationDirectory()) : app_dir);
    if (try_load_constants_from(exe_relative_path, vc)) {
        return vc;
    }
    if (exe_relative_path != kValidationConstantsPath &&
        try_load_constants_from(kValidationConstantsPath, vc)) {
        return vc;
    }
    TraceLog(LOG_WARNING,
             "Validation constants not found or invalid; using compiled defaults (tried '%s'%s)",
             exe_relative_path.c_str(),
             exe_relative_path == kValidationConstantsPath ? "" : " and 'content/constants.json'");
    return vc;
}

static std::optional<ObstacleKind> parse_kind(const std::string& s) {
    if (s == "shape_gate")       return ObstacleKind::ShapeGate;
    if (s == "lane_block")       return ObstacleKind::LaneBlock;
    if (s == "combo_gate")       return ObstacleKind::ComboGate;
    if (s == "split_path")       return ObstacleKind::SplitPath;
    return std::nullopt;
}

static std::optional<Shape> parse_shape(const std::string& s) {
    if (s == "circle")   return Shape::Circle;
    if (s == "square")   return Shape::Square;
    if (s == "triangle") return Shape::Triangle;
    return std::nullopt;
}

bool parse_beat_map(const std::string& json_str, BeatMap& out,
                    std::vector<BeatMapError>& errors,
                    const std::string& difficulty) {
    json j;
    try {
        j = json::parse(json_str);
    } catch (const json::parse_error& e) {
        errors.push_back({-1, std::string("JSON parse error: ") + e.what()});
        return false;
    }

    // ── Metadata ─────────────────────────────────────────────
    out.beats.clear();  // reset before every parse to prevent stale entries on reuse
    out.beat_times.clear();
    out.song_id    = j.value("song_id", "");
    out.title      = j.value("title", "");
    out.bpm        = j.value("bpm", 120.0f);
    out.offset     = j.value("offset", 0.0f);
    out.lead_beats = j.value("lead_beats", 4);
    out.duration   = j.value("duration_sec", 180.0f);
    out.difficulty = difficulty;

    if (j.contains("beat_times") && j["beat_times"].is_array()) {
        for (const auto& t : j["beat_times"]) {
            if (t.is_number_float() || t.is_number_integer()) {
                out.beat_times.push_back(t.get<float>());
            }
        }
    }

    // ── song_path: explicit field, or derive from song_id ────
    if (j.contains("song_path") && j["song_path"].is_string()) {
        out.song_path = j["song_path"].get<std::string>();
    } else if (!out.song_id.empty()) {
        out.song_path = "content/audio/" + out.song_id + ".flac";
    }

    // ── Beats: nested difficulties (preferred) or flat array ─
    const json* beats_array = nullptr;

    if (j.contains("difficulties") && j["difficulties"].is_object()) {
        const auto& diffs = j["difficulties"];
        if (diffs.contains(difficulty) && diffs[difficulty].contains("beats")
            && diffs[difficulty]["beats"].is_array()) {
            beats_array = &diffs[difficulty]["beats"];
        } else {
            // Requested difficulty not found — try fallback order
            const char* fallbacks[] = {"medium", "easy", "hard"};
            for (const char* fb : fallbacks) {
                if (diffs.contains(fb) && diffs[fb].contains("beats")
                    && diffs[fb]["beats"].is_array()) {
                    beats_array = &diffs[fb]["beats"];
                    out.difficulty = fb;
                    errors.push_back({-1, std::string("Difficulty '") + difficulty
                        + "' not found, falling back to '" + fb + "'"});
                    break;
                }
            }
        }
    }

    // Backward compat: flat top-level beats array
    if (!beats_array && j.contains("beats") && j["beats"].is_array()) {
        beats_array = &j["beats"];
    }

    if (!beats_array) {
        errors.push_back({-1, "No beats found in beatmap"});
        return false;
    }

    // ── Parse individual beat entries ────────────────────────
    bool parse_ok = true;
    for (const auto& b : *beats_array) {
        BeatEntry entry;
        entry.beat_index = b.value("beat", 0);

        std::string kind_str = b.value("kind", "shape_gate");
        auto kind_opt = parse_kind(kind_str);
        if (!kind_opt) {
            errors.push_back({entry.beat_index,
                "Unknown obstacle kind '" + kind_str + "' at beat " + std::to_string(entry.beat_index)});
            parse_ok = false;
            continue;
        }
        entry.kind = *kind_opt;

        if (b.contains("shape")) {
            std::string shape_str = b["shape"].get<std::string>();
            auto shape_opt = parse_shape(shape_str);
            if (!shape_opt) {
                errors.push_back({entry.beat_index,
                    "Unknown shape '" + shape_str + "' at beat " + std::to_string(entry.beat_index)});
                parse_ok = false;
                continue;
            }
            entry.shape = *shape_opt;
        } else if (kind_requires_shape(entry.kind)) {
            errors.push_back({entry.beat_index,
                "Missing required shape for obstacle kind '" + kind_str + "' at beat "
                    + std::to_string(entry.beat_index)});
            parse_ok = false;
            continue;
        }

        entry.lane = static_cast<int8_t>(b.value("lane", 1));

        const float grid_time = out.offset + entry.beat_index * (60.0f / out.bpm);
        float beat_time = grid_time;
        if (!out.beat_times.empty() &&
            entry.beat_index >= 0 &&
            static_cast<size_t>(entry.beat_index) < out.beat_times.size()) {
            beat_time = out.beat_times[static_cast<size_t>(entry.beat_index)];
        }

        const bool has_time_sec = b.contains("time_sec") &&
                                  (b["time_sec"].is_number_float() || b["time_sec"].is_number_integer());
        entry.has_time_sec = has_time_sec;
        entry.time_sec = has_time_sec ? b["time_sec"].get<float>() : beat_time;

        const std::string timing_source = b.value("timing_source", "");
        if (has_time_sec && timing_source != "onset" && !out.beat_times.empty() &&
            entry.beat_index >= 0 &&
            static_cast<size_t>(entry.beat_index) < out.beat_times.size()) {
            constexpr float kBeatTimeMismatchWarnSec = 0.010f;
            if (std::fabs(entry.time_sec - beat_time) > kBeatTimeMismatchWarnSec) {
                TraceLog(LOG_WARNING,
                         "Beat time mismatch at beat=%d: time_sec=%.6f, beat_times[%d]=%.6f",
                         entry.beat_index, entry.time_sec, entry.beat_index, beat_time);
            }
        }

        if (b.contains("blocked")) {
            if (!b["blocked"].is_array()) {
                errors.push_back({entry.beat_index,
                    "'blocked' must be an array at beat " + std::to_string(entry.beat_index)});
                parse_ok = false;
                continue;
            }
            entry.blocked_mask = 0;
            bool blocked_ok = true;
            for (const auto& lane_idx : b["blocked"]) {
                if (!lane_idx.is_number_integer()) {
                    errors.push_back({entry.beat_index,
                        "'blocked' entries must be integer lane indices at beat "
                        + std::to_string(entry.beat_index)});
                    parse_ok = false;
                    blocked_ok = false;
                    break;
                }
                const int lane = lane_idx.get<int>();
                if (lane < 0 || lane > 2) {
                    errors.push_back({entry.beat_index,
                        "'blocked' lane index must be in range [0, 2] at beat "
                        + std::to_string(entry.beat_index)});
                    parse_ok = false;
                    blocked_ok = false;
                    break;
                }
                entry.blocked_mask |= static_cast<uint8_t>(1 << lane);
            }
            if (!blocked_ok) continue;
        }

        out.beats.push_back(entry);
    }

    if (!parse_ok) return false;

    // Sort beats by beat_index; for ties, sort by resolved time_sec so
    // authored timing within the same beat stays schedulable in-order.
    // Stable sort preserves authored order when keys are identical.
    std::stable_sort(out.beats.begin(), out.beats.end(),
                     [](const BeatEntry& a, const BeatEntry& b) {
                         if (a.beat_index != b.beat_index) {
                             return a.beat_index < b.beat_index;
                         }
                         if (a.time_sec != b.time_sec) {
                             return a.time_sec < b.time_sec;
                         }
                         return false;
                     });

    if (out.beat_times.empty() && !out.beats.empty()) {
        const int max_beat = out.beats.back().beat_index;
        if (max_beat >= 0) {
            out.beat_times.reserve(static_cast<size_t>(max_beat + 1));
            const float beat_period = 60.0f / out.bpm;
            for (int i = 0; i <= max_beat; ++i) {
                out.beat_times.push_back(out.offset + static_cast<float>(i) * beat_period);
            }
        }
    }

    return true;
}

bool load_beat_map(const std::string& json_path, BeatMap& out,
                   std::vector<BeatMapError>& errors,
                   const std::string& difficulty) {
    char* file_text = LoadFileText(json_path.c_str());
    if (file_text == nullptr) {
        errors.push_back({-1, "Could not open file: " + json_path});
        return false;
    }

    const std::string content(file_text);
    UnloadFileText(file_text);
    return parse_beat_map(content, out, errors, difficulty);
}

bool validate_beat_map(const BeatMap& map, std::vector<BeatMapError>& errors) {
    return validate_beat_map(map, errors, load_validation_constants());
}

bool validate_beat_map(const BeatMap& map, std::vector<BeatMapError>& errors,
                       const ValidationConstants& vc) {
    bool valid = true;

    // Rule 7: BPM in range
    if (map.bpm < vc.bpm_min || map.bpm > vc.bpm_max) {
        errors.push_back({-1, "BPM must be in range [" + std::to_string(static_cast<int>(vc.bpm_min))
            + ", " + std::to_string(static_cast<int>(vc.bpm_max)) + "], got " + std::to_string(map.bpm)});
        valid = false;
    }

    // Rule 8: offset in range
    if (map.offset < vc.offset_min || map.offset > vc.offset_max) {
        errors.push_back({-1, "Offset must be in range [" + std::to_string(vc.offset_min)
            + ", " + std::to_string(vc.offset_max) + "], got " + std::to_string(map.offset)});
        valid = false;
    }

    // Rule 9: lead_beats in range
    if (map.lead_beats < vc.lead_beats_min || map.lead_beats > vc.lead_beats_max) {
        errors.push_back({-1, "lead_beats must be in range [" + std::to_string(vc.lead_beats_min)
            + ", " + std::to_string(vc.lead_beats_max) + "], got " + std::to_string(map.lead_beats)});
        valid = false;
    }

    // Rule 10: at least 1 beat entry
    if (map.beats.empty()) {
        errors.push_back({-1, "Beat map must have at least 1 beat entry"});
        valid = false;
    }

    float beat_period = 60.0f / map.bpm;
    int max_beat = static_cast<int>(std::floor(map.duration / beat_period));

    int prev_beat = -1;
    int prev_shape_beat = -1;
    Shape prev_shape = Shape::Circle;
    bool prev_had_shape = false;
    bool has_prev_authored_time = false;
    float prev_authored_time = 0.0f;
    bool has_prev_resolved_time_for_beat = false;
    float prev_resolved_time_for_beat = 0.0f;

    for (size_t i = 0; i < map.beats.size(); ++i) {
        const auto& entry = map.beats[i];
        const float grid_time = map.offset + static_cast<float>(entry.beat_index) * beat_period;
        float resolved_time = grid_time;
        if (entry.has_time_sec) {
            resolved_time = entry.time_sec;
        } else if (!map.beat_times.empty() &&
                   entry.beat_index >= 0 &&
                   static_cast<size_t>(entry.beat_index) < map.beat_times.size()) {
            resolved_time = map.beat_times[static_cast<size_t>(entry.beat_index)];
        }

        // Rule 1: beat indices monotonically non-decreasing
        if (entry.beat_index < prev_beat && prev_beat >= 0) {
            errors.push_back({entry.beat_index, "Beat indices must be monotonically non-decreasing"});
            valid = false;
        }

        if (entry.beat_index == prev_beat) {
            if (has_prev_resolved_time_for_beat && resolved_time < prev_resolved_time_for_beat) {
                errors.push_back({entry.beat_index,
                    "Entries sharing a beat index must have non-decreasing resolved time"});
                valid = false;
            }
            prev_resolved_time_for_beat = resolved_time;
            has_prev_resolved_time_for_beat = true;
        } else {
            prev_beat = entry.beat_index;
            prev_resolved_time_for_beat = resolved_time;
            has_prev_resolved_time_for_beat = true;
        }

        // Rule 2: no beat index beyond song duration
        if (entry.beat_index > max_beat) {
            errors.push_back({entry.beat_index, "Beat index exceeds song duration"});
            valid = false;
        }

        // Rule 2b: beat_index must reference a loaded timestamp when beat_times are present
        if (!map.beat_times.empty() &&
            (entry.beat_index < 0 || static_cast<size_t>(entry.beat_index) >= map.beat_times.size())) {
            errors.push_back({entry.beat_index, "Beat index is out of range for beat_times array"});
            valid = false;
        }

        // Rule 5: shape_gate / split_path must have lane 0-2
        if ((entry.kind == ObstacleKind::ShapeGate || entry.kind == ObstacleKind::SplitPath) &&
            (entry.lane < 0 || entry.lane > 2)) {
            errors.push_back({entry.beat_index, "Lane must be 0-2"});
            valid = false;
        }

        if ((entry.kind == ObstacleKind::LaneBlock || entry.kind == ObstacleKind::ComboGate) &&
            !has_only_lane_bits(entry.blocked_mask)) {
            errors.push_back({entry.beat_index, "blocked_mask must only use lane bits 0..2"});
            valid = false;
        }

        if (entry.kind == ObstacleKind::LaneBlock && lane_bit_count(entry.blocked_mask) != 1) {
            errors.push_back({entry.beat_index, "LaneBlock blocked_mask must contain exactly one lane bit"});
            valid = false;
        }

        // Rule 5b: combo_gate blocked_mask must block at least one lane and leave at least one open
        if (entry.kind == ObstacleKind::ComboGate) {
            if (entry.blocked_mask == 0) {
                errors.push_back({entry.beat_index, "ComboGate must block at least one lane"});
                valid = false;
            } else if (entry.blocked_mask == 0x07) {
                errors.push_back({entry.beat_index, "ComboGate must leave at least one lane open"});
                valid = false;
            }
        }

        if (entry.has_time_sec) {
            if (!std::isfinite(entry.time_sec)) {
                errors.push_back({entry.beat_index, "time_sec must be finite when provided"});
                valid = false;
            } else {
                if (entry.time_sec < 0.0f) {
                    errors.push_back({entry.beat_index, "time_sec must be >= 0 when provided"});
                    valid = false;
                }
                if (entry.time_sec > map.duration) {
                    errors.push_back({entry.beat_index, "time_sec must be <= duration_sec when provided"});
                    valid = false;
                }
                if (has_prev_authored_time && entry.time_sec < prev_authored_time) {
                    errors.push_back({entry.beat_index,
                        "time_sec must be non-decreasing across authored beat entries"});
                    valid = false;
                }
                prev_authored_time = entry.time_sec;
                has_prev_authored_time = true;
            }
        }

        // Rule 6: different-shape gates must be >= min_shape_change_gap beats apart
        bool has_shape = kind_requires_shape(entry.kind);
        if (has_shape && prev_had_shape &&
            entry.shape != prev_shape &&
            (entry.beat_index - prev_shape_beat) < vc.min_shape_change_gap) {
            errors.push_back({entry.beat_index,
                "Different-shape gates must be >= " + std::to_string(vc.min_shape_change_gap) + " beats apart"});
            valid = false;
        }
        if (has_shape) {
            prev_shape_beat = entry.beat_index;
            prev_shape = entry.shape;
            prev_had_shape = true;
        }
    }

    return valid;
}

void init_song_state(SongState& state, const BeatMap& map) {
    state.bpm          = map.bpm;
    state.offset       = map.offset;
    state.lead_beats   = map.lead_beats;
    state.duration_sec = map.duration;
    state.song_time    = 0.0f;
    state.current_beat = -1;
    state.playing      = false;
    state.finished     = false;
    state.next_spawn_idx = 0;
    song_state_compute_derived(state);
}
