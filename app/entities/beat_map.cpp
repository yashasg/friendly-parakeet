#include "beat_map.h"
#include "../util/app_dir_path.h"
#include "../util/rhythm_math.h"
#include "../util/shape_lane_mapping.h"
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <raylib.h>

using json = nlohmann::json;
namespace {
constexpr const char* kValidationConstantsPath = "content/constants.json";

std::string type_error_message(const char* field, const char* expected, const json& value) {
    return std::string("'") + field + "' must be " + expected + " (got " + value.type_name() + ")";
}

void push_type_error(std::vector<BeatMapError>& errors,
                     const int beat_index,
                     const char* field,
                     const char* expected,
                     const json& value) {
    errors.push_back({beat_index, type_error_message(field, expected, value)});
}

bool read_optional_string(const json& object,
                          const char* field,
                          std::string& out,
                          std::vector<BeatMapError>& errors,
                          const int beat_index) {
    if (!object.contains(field)) return true;
    const auto& value = object[field];
    if (!value.is_string()) {
        push_type_error(errors, beat_index, field, "a string", value);
        return false;
    }
    out = value.get<std::string>();
    return true;
}

bool read_optional_float(const json& object,
                         const char* field,
                         float& out,
                         std::vector<BeatMapError>& errors,
                         const int beat_index) {
    if (!object.contains(field)) return true;
    const auto& value = object[field];
    if (!value.is_number()) {
        push_type_error(errors, beat_index, field, "a number", value);
        return false;
    }
    out = value.get<float>();
    return true;
}

bool read_int_value(const json& value,
                    const char* field,
                    int& out,
                    std::vector<BeatMapError>& errors,
                    const int beat_index) {
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        push_type_error(errors, beat_index, field, "an integer", value);
        return false;
    }
    if (value.is_number_unsigned()) {
        const auto raw = value.get<std::uint64_t>();
        if (raw > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
            errors.push_back({beat_index, std::string("'") + field + "' must fit in a 32-bit integer"});
            return false;
        }
        out = static_cast<int>(raw);
        return true;
    }

    const auto raw = value.get<std::int64_t>();
    if (raw < static_cast<std::int64_t>(std::numeric_limits<int>::min()) ||
        raw > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
        errors.push_back({beat_index, std::string("'") + field + "' must fit in a 32-bit integer"});
        return false;
    }
    out = static_cast<int>(raw);
    return true;
}

bool read_optional_int(const json& object,
                       const char* field,
                       int& out,
                       std::vector<BeatMapError>& errors,
                       const int beat_index) {
    if (!object.contains(field)) return true;
    return read_int_value(object[field], field, out, errors, beat_index);
}

bool validate_blocked_lanes_field(const json& object,
                                  std::vector<BeatMapError>& errors,
                                  const int beat_index) {
    if (!object.contains("blocked")) return true;

    const auto& blocked = object["blocked"];
    if (!blocked.is_array()) {
        push_type_error(errors, beat_index, "blocked", "an array", blocked);
        return false;
    }

    bool ok = true;
    for (const auto& lane_json : blocked) {
        int lane = 0;
        if (!read_int_value(lane_json, "blocked[]", lane, errors, beat_index)) {
            ok = false;
            continue;
        }
        if (lane < 0 || lane > 2) {
            errors.push_back({beat_index,
                "'blocked[]' lane must be in range [0, 2] at beat "
                + std::to_string(beat_index)});
            ok = false;
        }
    }

    return ok;
}

std::optional<int> max_derived_beat_for_metadata(const float bpm, const float duration) {
    if (!std::isfinite(bpm) || !std::isfinite(duration) || bpm <= 0.0f || duration < 0.0f) {
        return std::nullopt;
    }

    const double beat_period = 60.0 / static_cast<double>(bpm);
    if (!std::isfinite(beat_period) || beat_period <= 0.0) {
        return std::nullopt;
    }

    const double max_beat = std::floor(static_cast<double>(duration) / beat_period);
    if (!std::isfinite(max_beat) || max_beat < 0.0 ||
        max_beat > static_cast<double>(std::numeric_limits<int>::max())) {
        return std::nullopt;
    }

    return static_cast<int>(max_beat);
}
} // namespace

entt::entity create_beat_map_entity(entt::registry& reg, BeatMap map) {
    auto existing = reg.view<BeatMapTag>();
    if (existing.begin() != existing.end()) {
        throw std::logic_error("BeatMapTag entity already exists");
    }

    auto entity = reg.create();
    reg.emplace<BeatMapTag>(entity);
    reg.emplace<BeatMap>(entity, std::move(map));
    return entity;
}

BeatMap* find_beat_map(entt::registry& reg) {
    auto view = reg.view<BeatMapTag, BeatMap>();
    auto it = view.begin();
    if (it == view.end()) {
        return nullptr;
    }
    const auto entity = *it;
    if (++it != view.end()) {
        throw std::logic_error("multiple BeatMap entities exist");
    }
    return &view.get<BeatMap>(entity);
}

const BeatMap* find_beat_map(const entt::registry& reg) {
    auto view = reg.view<BeatMapTag, const BeatMap>();
    auto it = view.begin();
    if (it == view.end()) {
        return nullptr;
    }
    const auto entity = *it;
    if (++it != view.end()) {
        throw std::logic_error("multiple BeatMap entities exist");
    }
    return &view.get<const BeatMap>(entity);
}

BeatMap& beat_map(entt::registry& reg) {
    if (auto* map = find_beat_map(reg)) {
        return *map;
    }
    throw std::logic_error("BeatMap entity is missing; call create_beat_map_entity() first");
}

const BeatMap& beat_map(const entt::registry& reg) {
    if (const auto* map = find_beat_map(reg)) {
        return *map;
    }
    throw std::logic_error("BeatMap entity is missing; call create_beat_map_entity() first");
}

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
    const std::string exe_relative_path = util::join_app_dir(
        app_dir.empty() ? std::string_view(GetApplicationDirectory()) : std::string_view(app_dir),
        kValidationConstantsPath);
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

static std::optional<Shape> parse_shape(const std::string& s) {
    if (s == "circle")   return Shape::Circle;
    if (s == "square")   return Shape::Square;
    if (s == "triangle") return Shape::Triangle;
    if (s == "hexagon")  return Shape::Hexagon;
    return std::nullopt;
}

// ── Per-(kind, shape) beat-entry sinks (issue #1202/#1204) ──────────
// The former `BeatEntry::shape` discriminator is encoded by which per-shape
// vector receives the entry. The sink table is indexed by `shape_index()`;
// Shape::Hexagon is not a valid required shape for shape_gate / split_path,
// so its slot points at `null_shape_sink` (shared between both tables) —
// the loader treats nullptr as a parse error and emits a "Shape 'hexagon'
// is not a valid required shape" message.
using ShapeBinAccessor = std::vector<BeatEntry>* (*)(BeatMap&);

std::vector<BeatEntry>* sg_circle_sink  (BeatMap& m) { return &m.shape_gate_circle_beats; }
std::vector<BeatEntry>* sg_square_sink  (BeatMap& m) { return &m.shape_gate_square_beats; }
std::vector<BeatEntry>* sg_triangle_sink(BeatMap& m) { return &m.shape_gate_triangle_beats; }
std::vector<BeatEntry>* sp_circle_sink  (BeatMap& m) { return &m.split_path_circle_beats; }
std::vector<BeatEntry>* sp_square_sink  (BeatMap& m) { return &m.split_path_square_beats; }
std::vector<BeatEntry>* sp_triangle_sink(BeatMap& m) { return &m.split_path_triangle_beats; }
std::vector<BeatEntry>* null_shape_sink (BeatMap& m) { (void)m; return nullptr; }

inline constexpr std::array<ShapeBinAccessor, kShapeCount> kShapeGateSinks{
    &sg_circle_sink, &sg_square_sink, &sg_triangle_sink, &null_shape_sink
};
inline constexpr std::array<ShapeBinAccessor, kShapeCount> kSplitPathSinks{
    &sp_circle_sink, &sp_square_sink, &sp_triangle_sink, &null_shape_sink
};

// ── Per-kind beat-entry sinks (issue #1202/#1204) ───────────
// Parsing dispatches into per-kind vectors directly by the JSON `kind`
// string. No discriminator enum is reintroduced — see `parse_beat_map`
// below where each known kind string has its own dispatch branch.
//
// `shape_gate` and `split_path` rows both require a shape + lane payload;
// `onset_marker` rows require neither. The two payload fields are validated
// independently in `parse_beat_map`, so this single predicate covers both
// "missing shape" and "missing lane" checks (issue #1414).
bool kind_string_requires_payload(std::string_view kind_str) {
    return kind_str == "shape_gate" || kind_str == "split_path";
}

bool kind_string_is_supported(std::string_view kind_str) {
    return kind_str == "shape_gate" ||
           kind_str == "split_path" ||
           kind_str == "onset_marker";
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

    if (!j.is_object()) {
        errors.push_back({-1, type_error_message("root", "an object", j)});
        return false;
    }

    // ── Metadata ─────────────────────────────────────────────
    out.shape_gate_circle_beats.clear();
    out.shape_gate_square_beats.clear();
    out.shape_gate_triangle_beats.clear();
    out.split_path_circle_beats.clear();
    out.split_path_square_beats.clear();
    out.split_path_triangle_beats.clear();
    out.onset_marker_beats.clear();
    out.beat_times.clear();
    out.song_id.clear();
    out.title.clear();
    out.song_path.clear();
    out.bpm = 120.0f;
    out.offset = 0.0f;
    out.lead_beats = 4;
    out.duration = 180.0f;
    bool parse_ok = true;
    parse_ok &= read_optional_string(j, "song_id", out.song_id, errors, -1);
    parse_ok &= read_optional_string(j, "title", out.title, errors, -1);
    parse_ok &= read_optional_float(j, "bpm", out.bpm, errors, -1);
    parse_ok &= read_optional_float(j, "offset", out.offset, errors, -1);
    parse_ok &= read_optional_int(j, "lead_beats", out.lead_beats, errors, -1);
    parse_ok &= read_optional_float(j, "duration_sec", out.duration, errors, -1);
    out.difficulty = difficulty;

    if (!std::isfinite(out.bpm) || out.bpm <= 0.0f) {
        errors.push_back({-1, "BPM must be finite and > 0 before deriving beat timing"});
        return false;
    }
    if (!std::isfinite(out.offset)) {
        errors.push_back({-1, "Offset must be finite before deriving beat timing"});
        return false;
    }
    if (!std::isfinite(out.duration)) {
        errors.push_back({-1, "duration_sec must be finite before deriving beat timing"});
        return false;
    }
    if (out.lead_beats <= 0) {
        errors.push_back({-1, "lead_beats must be > 0 before deriving song timing"});
        return false;
    }

    if (j.contains("beat_times")) {
        if (!j["beat_times"].is_array()) {
            push_type_error(errors, -1, "beat_times", "an array", j["beat_times"]);
            parse_ok = false;
        } else {
            bool has_prev_beat_time = false;
            float prev_beat_time = 0.0f;
            size_t beat_time_index = 0;
            for (const auto& t : j["beat_times"]) {
                if (!t.is_number()) {
                    push_type_error(errors, -1, "beat_times[]", "a number", t);
                    parse_ok = false;
                    ++beat_time_index;
                    continue;
                }
                const float beat_time = t.get<float>();
                if (!std::isfinite(beat_time)) {
                    errors.push_back({-1, "beat_times[" + std::to_string(beat_time_index) + "] must be finite"});
                    parse_ok = false;
                    ++beat_time_index;
                    continue;
                }
                if (has_prev_beat_time && beat_time < prev_beat_time) {
                    errors.push_back({-1, "beat_times must be non-decreasing"});
                    parse_ok = false;
                }
                out.beat_times.push_back(beat_time);
                prev_beat_time = beat_time;
                has_prev_beat_time = true;
                ++beat_time_index;
            }
        }
    }

    // ── song_path: explicit field, or derive from song_id ────
    if (j.contains("song_path")) {
        parse_ok &= read_optional_string(j, "song_path", out.song_path, errors, -1);
    } else if (!out.song_id.empty()) {
        out.song_path = "content/audio/" + out.song_id + ".flac";
    }

    // ── Beats: nested difficulties (preferred) or flat array ─
    const json* beats_array = nullptr;

    if (j.contains("difficulties") && j["difficulties"].is_object()) {
        const auto& diffs = j["difficulties"];
        const auto diff_it = diffs.find(difficulty);
        if (diff_it != diffs.end()) {
            if (!diff_it->is_object()) {
                errors.push_back({-1, std::string("Difficulty '") + difficulty
                    + "' must be an object (got " + diff_it->type_name() + ")"});
                return false;
            }
            const auto beats_it = diff_it->find("beats");
            if (beats_it == diff_it->end()) {
                errors.push_back({-1, std::string("Difficulty '") + difficulty
                    + "' must contain a 'beats' array"});
                return false;
            }
            if (!beats_it->is_array()) {
                errors.push_back({-1, std::string("Difficulty '") + difficulty
                    + "' field 'beats' must be an array (got " + beats_it->type_name() + ")"});
                return false;
            }
            beats_array = &(*beats_it);
        } else {
            // Requested difficulty not found — try fallback order
            const char* fallbacks[] = {"medium", "easy", "hard"};
            for (const char* fb : fallbacks) {
                const auto fallback_it = diffs.find(fb);
                if (fallback_it != diffs.end() && fallback_it->is_object()
                    && fallback_it->contains("beats") && (*fallback_it)["beats"].is_array()) {
                    beats_array = &(*fallback_it)["beats"];
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
    for (const auto& b : *beats_array) {
        if (!b.is_object()) {
            push_type_error(errors, -1, "beats[]", "an object", b);
            parse_ok = false;
            continue;
        }

        BeatEntry entry;
        Shape    parsed_shape   = Shape::Hexagon;
        bool     has_parsed_shape = false;
        if (!b.contains("beat")) {
            errors.push_back({-1, "Missing required beat for obstacle entry"});
            parse_ok = false;
            continue;
        }
        if (!read_optional_int(b, "beat", entry.beat_index, errors, -1)) {
            parse_ok = false;
            continue;
        }

        std::string kind_str = "shape_gate";
        if (!read_optional_string(b, "kind", kind_str, errors, entry.beat_index)) {
            parse_ok = false;
            continue;
        }
        if (!kind_string_is_supported(kind_str)) {
            errors.push_back({entry.beat_index,
                "Unknown obstacle kind '" + kind_str + "' at beat " + std::to_string(entry.beat_index)});
            parse_ok = false;
            continue;
        }

        if (b.contains("shape")) {
            std::string shape_str;
            if (!read_optional_string(b, "shape", shape_str, errors, entry.beat_index)) {
                parse_ok = false;
                continue;
            }
            auto shape_opt = parse_shape(shape_str);
            if (!shape_opt) {
                errors.push_back({entry.beat_index,
                    "Unknown shape '" + shape_str + "' at beat " + std::to_string(entry.beat_index)});
                parse_ok = false;
                continue;
            }
            parsed_shape = *shape_opt;
            has_parsed_shape = true;
        } else if (kind_string_requires_payload(kind_str)) {
            errors.push_back({entry.beat_index,
                "Missing required shape for obstacle kind '" + kind_str + "' at beat "
                    + std::to_string(entry.beat_index)});
            parse_ok = false;
            continue;
        }

        int lane_value = entry.lane;
        if (!read_optional_int(b, "lane", lane_value, errors, entry.beat_index)) {
            parse_ok = false;
            continue;
        }
        if (!b.contains("lane") && kind_string_requires_payload(kind_str)) {
            errors.push_back({entry.beat_index,
                "Missing required lane for obstacle kind '" + kind_str + "' at beat "
                    + std::to_string(entry.beat_index)});
            parse_ok = false;
            continue;
        }
        if (b.contains("lane") && (lane_value < 0 || lane_value > 2)) {
            errors.push_back({entry.beat_index,
                "'lane' must be in range [0, 2] at beat " + std::to_string(entry.beat_index)});
            parse_ok = false;
            continue;
        }
        entry.lane = static_cast<int8_t>(lane_value);

        const float grid_time = out.offset + entry.beat_index * (60.0f / out.bpm);
        float beat_time = grid_time;
        if (!out.beat_times.empty() &&
            entry.beat_index >= 0 &&
            static_cast<size_t>(entry.beat_index) < out.beat_times.size()) {
            beat_time = out.beat_times[static_cast<size_t>(entry.beat_index)];
        }

        const bool has_time_sec = b.contains("time_sec");
        entry.has_time_sec = has_time_sec;
        entry.time_sec = beat_time;
        if (has_time_sec && !read_optional_float(b, "time_sec", entry.time_sec, errors, entry.beat_index)) {
            parse_ok = false;
            continue;
        }

        std::string timing_source;
        if (!read_optional_string(b, "timing_source", timing_source, errors, entry.beat_index)) {
            parse_ok = false;
            continue;
        }
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

        if (!validate_blocked_lanes_field(b, errors, entry.beat_index)) {
            parse_ok = false;
            continue;
        }
        if (b.contains("blocked")) {
            errors.push_back({entry.beat_index,
                "'blocked' is not supported by active beatmap obstacle kinds at beat "
                + std::to_string(entry.beat_index)});
            parse_ok = false;
            continue;
        }

        // Per-(kind, shape) dispatch into the appropriate vector (issue
        // #1202/#1204). No discriminator enum survives in the data model —
        // the row lives in exactly one of seven per-(kind, shape) tables.
        // Hexagon is not a valid required shape for shape_gate / split_path;
        // the sink table returns nullptr in that slot so the loader rejects
        // it explicitly.
        if (kind_str == "onset_marker") {
            out.onset_marker_beats.push_back(entry);
        } else {
            const auto& sinks = (kind_str == "shape_gate") ? kShapeGateSinks
                                                           : kSplitPathSinks;
            const int idx = shape_index(parsed_shape);
            std::vector<BeatEntry>* sink = (idx >= 0) ? sinks[static_cast<size_t>(idx)](out)
                                                      : nullptr;
            if (!sink) {
                errors.push_back({entry.beat_index,
                    "Shape 'hexagon' is not a valid required shape for obstacle kind '"
                    + kind_str + "' at beat " + std::to_string(entry.beat_index)});
                parse_ok = false;
                continue;
            }
            sink->push_back(entry);
        }
        (void)has_parsed_shape;
    }

    if (!parse_ok) return false;

    // Sort each per-(kind, shape) vector by beat_index; for ties, sort by
    // resolved time_sec so authored timing within the same beat stays
    // schedulable in-order. Stable sort preserves authored order when keys
    // are identical.
    auto by_beat = [](const BeatEntry& a, const BeatEntry& b) {
        if (a.beat_index != b.beat_index) {
            return a.beat_index < b.beat_index;
        }
        if (a.time_sec != b.time_sec) {
            return a.time_sec < b.time_sec;
        }
        return false;
    };
    auto sort_bin = [&](std::vector<BeatEntry>& v) { std::stable_sort(v.begin(), v.end(), by_beat); };
    sort_bin(out.shape_gate_circle_beats);
    sort_bin(out.shape_gate_square_beats);
    sort_bin(out.shape_gate_triangle_beats);
    sort_bin(out.split_path_circle_beats);
    sort_bin(out.split_path_square_beats);
    sort_bin(out.split_path_triangle_beats);
    sort_bin(out.onset_marker_beats);

    if (out.beat_times.empty()) {
        int max_beat = -1;
        auto consider_max = [&](const std::vector<BeatEntry>& v) {
            if (!v.empty()) max_beat = std::max(max_beat, v.back().beat_index);
        };
        consider_max(out.shape_gate_circle_beats);
        consider_max(out.shape_gate_square_beats);
        consider_max(out.shape_gate_triangle_beats);
        consider_max(out.split_path_circle_beats);
        consider_max(out.split_path_square_beats);
        consider_max(out.split_path_triangle_beats);
        consider_max(out.onset_marker_beats);

        if (max_beat >= 0) {
            const auto max_derived_beat = max_derived_beat_for_metadata(out.bpm, out.duration);
            if (!max_derived_beat || max_beat > *max_derived_beat) {
                errors.push_back({max_beat,
                    "Beat index exceeds safe derived beat_times range"});
                return false;
            }

            out.beat_times.reserve(static_cast<size_t>(max_beat) + 1U);
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

bool load_and_validate_beat_map(const std::string& json_path, BeatMap& out,
                                std::vector<BeatMapError>& errors,
                                const std::string& difficulty) {
    BeatMap candidate;
    if (!load_beat_map(json_path, candidate, errors, difficulty)) {
        return false;
    }
    if (!validate_beat_map(candidate, errors)) {
        return false;
    }
    out = std::move(candidate);
    return true;
}

bool validate_beat_map(const BeatMap& map, std::vector<BeatMapError>& errors) {
    return validate_beat_map(map, errors, load_validation_constants());
}

bool validate_beat_map(const BeatMap& map, std::vector<BeatMapError>& errors,
                       const ValidationConstants& vc) {
    bool valid = true;
    bool timing_metadata_valid = true;

    // Rule 7: BPM in range
    if (!std::isfinite(map.bpm)) {
        errors.push_back({-1, "BPM must be finite"});
        valid = false;
        timing_metadata_valid = false;
    } else if (map.bpm < vc.bpm_min || map.bpm > vc.bpm_max) {
        errors.push_back({-1, "BPM must be in range [" + std::to_string(static_cast<int>(vc.bpm_min))
            + ", " + std::to_string(static_cast<int>(vc.bpm_max)) + "], got " + std::to_string(map.bpm)});
        valid = false;
        timing_metadata_valid = false;
    }

    // Rule 8: offset in range
    if (!std::isfinite(map.offset)) {
        errors.push_back({-1, "Offset must be finite"});
        valid = false;
        timing_metadata_valid = false;
    } else if (map.offset < vc.offset_min || map.offset > vc.offset_max) {
        errors.push_back({-1, "Offset must be in range [" + std::to_string(vc.offset_min)
            + ", " + std::to_string(vc.offset_max) + "], got " + std::to_string(map.offset)});
        valid = false;
        timing_metadata_valid = false;
    }

    if (!std::isfinite(map.duration)) {
        errors.push_back({-1, "duration_sec must be finite"});
        valid = false;
        timing_metadata_valid = false;
    } else if (map.duration < 0.0f) {
        errors.push_back({-1, "duration_sec must be >= 0"});
        valid = false;
        timing_metadata_valid = false;
    }

    // Rule 9: lead_beats in range
    if (map.lead_beats < vc.lead_beats_min || map.lead_beats > vc.lead_beats_max) {
        errors.push_back({-1, "lead_beats must be in range [" + std::to_string(vc.lead_beats_min)
            + ", " + std::to_string(vc.lead_beats_max) + "], got " + std::to_string(map.lead_beats)});
        valid = false;
    }

    // Rule 10: at least 1 beat entry across all per-(kind, shape) tables
    if (beat_map_empty(map)) {
        errors.push_back({-1, "Beat map must have at least 1 beat entry"});
        valid = false;
    }

    float beat_period = 0.0f;
    std::optional<int> max_beat;
    if (timing_metadata_valid) {
        const auto max_derived_beat = max_derived_beat_for_metadata(map.bpm, map.duration);
        if (!max_derived_beat) {
            errors.push_back({-1, "duration_sec produces an unsupported beat range"});
            valid = false;
            timing_metadata_valid = false;
        } else {
            beat_period = 60.0f / map.bpm;
            max_beat = *max_derived_beat;
        }
    }

    for (size_t i = 0; i < map.beat_times.size(); ++i) {
        const float beat_time = map.beat_times[i];
        if (!std::isfinite(beat_time)) {
            errors.push_back({-1, "beat_times[" + std::to_string(i) + "] must be finite"});
            valid = false;
            continue;
        }
        if (i > 0 && beat_time < map.beat_times[i - 1]) {
            errors.push_back({-1, "beat_times must be non-decreasing"});
            valid = false;
        }
    }

    // Per-row rules: walk each per-(kind, shape) vector once. Cross-table
    // ordering rules (Rule 1 — monotonic beat indices, intra-beat
    // resolved-time monotonicity, time_sec monotonicity) are evaluated by
    // walking a beat-index merge over the seven vectors below.
    //
    // Kind-specific rule references:
    //   has_shape       — true for shape_gate_* and split_path_* vectors
    //   requires_lane   — same set (onset_marker_beats default lane is
    //                     not validated against [0,2] semantics)
    auto validate_per_entry = [&](const BeatEntry& entry,
                                  bool has_shape, bool requires_lane) {
        // Rule 2: beat index must be within the playable song range
        if (entry.beat_index < 0) {
            errors.push_back({entry.beat_index, "Beat index must be non-negative"});
            valid = false;
        }
        if (max_beat && entry.beat_index > *max_beat) {
            errors.push_back({entry.beat_index, "Beat index exceeds song duration"});
            valid = false;
        }

        // Rule 2b: beat_index must reference a loaded timestamp when beat_times are present
        if (!map.beat_times.empty() &&
            (entry.beat_index < 0 || static_cast<size_t>(entry.beat_index) >= map.beat_times.size())) {
            errors.push_back({entry.beat_index, "Beat index is out of range for beat_times array"});
            valid = false;
        }

        // Rule 5: lane-bound obstacles must have lane 0-2
        if (requires_lane && (entry.lane < 0 || entry.lane > 2)) {
            errors.push_back({entry.beat_index, "Lane must be 0-2"});
            valid = false;
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
                if (std::isfinite(map.duration) && entry.time_sec > map.duration) {
                    errors.push_back({entry.beat_index, "time_sec must be <= duration_sec when provided"});
                    valid = false;
                }
            }
        }
        (void)has_shape;
    };

    for (const auto& entry : map.shape_gate_circle_beats)   validate_per_entry(entry, true,  true);
    for (const auto& entry : map.shape_gate_square_beats)   validate_per_entry(entry, true,  true);
    for (const auto& entry : map.shape_gate_triangle_beats) validate_per_entry(entry, true,  true);
    for (const auto& entry : map.split_path_circle_beats)   validate_per_entry(entry, true,  true);
    for (const auto& entry : map.split_path_square_beats)   validate_per_entry(entry, true,  true);
    for (const auto& entry : map.split_path_triangle_beats) validate_per_entry(entry, true,  true);
    for (const auto& entry : map.onset_marker_beats)        validate_per_entry(entry, false, false);

    // Cross-table ordering: walk the seven sorted vectors in beat-index
    // merge order so Rule 1 (monotonic), the intra-beat resolved-time
    // rule, and Rule 6 (different-shape gates) see the same total order
    // the parser produced.
    //
    // Each vector carries an int8_t shape_idx (0=Circle, 1=Square, 2=Triangle,
    // -1 for onset_marker which has no shape). Rule 6 compares shape_idx
    // values instead of a `Shape` field on the row — the discriminator
    // lives in the vector identity, not on the entry.
    struct MergeVectorRef {
        const std::vector<BeatEntry>* entries;
        size_t* cursor;
        int8_t shape_idx;
    };
    size_t i_sg_c = 0, i_sg_s = 0, i_sg_t = 0;
    size_t i_sp_c = 0, i_sp_s = 0, i_sp_t = 0;
    size_t i_om   = 0;
    const std::array<MergeVectorRef, 7> merge_vectors{{
        {&map.shape_gate_circle_beats,   &i_sg_c,  0},
        {&map.shape_gate_square_beats,   &i_sg_s,  1},
        {&map.shape_gate_triangle_beats, &i_sg_t,  2},
        {&map.split_path_circle_beats,   &i_sp_c,  0},
        {&map.split_path_square_beats,   &i_sp_s,  1},
        {&map.split_path_triangle_beats, &i_sp_t,  2},
        {&map.onset_marker_beats,        &i_om,   -1},
    }};

    int prev_beat = -1;
    int prev_shape_beat = -1;
    int8_t prev_shape_idx = -1;
    bool has_prev_authored_time = false;
    float prev_authored_time = 0.0f;
    bool has_prev_resolved_time_for_beat = false;
    float prev_resolved_time_for_beat = 0.0f;

    auto resolve_time = [&](const BeatEntry& entry) {
        float resolved_time = 0.0f;
        if (timing_metadata_valid) {
            resolved_time = map.offset + static_cast<float>(entry.beat_index) * beat_period;
        }
        if (entry.has_time_sec) {
            resolved_time = entry.time_sec;
        } else if (!map.beat_times.empty() &&
                   entry.beat_index >= 0 &&
                   static_cast<size_t>(entry.beat_index) < map.beat_times.size()) {
            resolved_time = map.beat_times[static_cast<size_t>(entry.beat_index)];
        }
        return resolved_time;
    };

    // Pick the next-smallest entry by (beat_index, time_sec) across the
    // seven sorted vectors. Each merge_vectors row advances its own cursor.
    auto step_merge = [&](int8_t& shape_idx_out) -> const BeatEntry* {
        const BeatEntry* best = nullptr;
        size_t* best_cursor = nullptr;
        int8_t  best_shape_idx = -1;
        for (const auto& v : merge_vectors) {
            if (*v.cursor >= v.entries->size()) continue;
            const auto* cand = &(*v.entries)[*v.cursor];
            if (!best ||
                cand->beat_index < best->beat_index ||
                (cand->beat_index == best->beat_index && cand->time_sec < best->time_sec)) {
                best = cand;
                best_cursor = v.cursor;
                best_shape_idx = v.shape_idx;
            }
        }
        if (best_cursor) (*best_cursor)++;
        shape_idx_out = best_shape_idx;
        return best;
    };

    int8_t entry_shape_idx = -1;
    while (const auto* entry_ptr = step_merge(entry_shape_idx)) {
        const auto& entry = *entry_ptr;
        const float resolved_time = resolve_time(entry);

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

        if (entry.has_time_sec && std::isfinite(entry.time_sec)) {
            if (has_prev_authored_time && entry.time_sec < prev_authored_time) {
                errors.push_back({entry.beat_index,
                    "time_sec must be non-decreasing across authored beat entries"});
                valid = false;
            }
            prev_authored_time = entry.time_sec;
            has_prev_authored_time = true;
        }

        // Rule 6: different-shape gates must be >= min_shape_change_gap beats apart.
        // shape_idx encodes the row's shape (0/1/2) or -1 for no-shape (onset).
        if (entry_shape_idx >= 0 && prev_shape_idx >= 0 &&
            entry_shape_idx != prev_shape_idx &&
            (entry.beat_index - prev_shape_beat) < vc.min_shape_change_gap) {
            errors.push_back({entry.beat_index,
                "Different-shape gates must be >= " + std::to_string(vc.min_shape_change_gap) + " beats apart"});
            valid = false;
        }
        if (entry_shape_idx >= 0) {
            prev_shape_beat = entry.beat_index;
            prev_shape_idx  = entry_shape_idx;
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
    state.next_shape_gate_circle_idx   = 0;
    state.next_shape_gate_square_idx   = 0;
    state.next_shape_gate_triangle_idx = 0;
    state.next_split_path_circle_idx   = 0;
    state.next_split_path_square_idx   = 0;
    state.next_split_path_triangle_idx = 0;
    state.next_onset_marker_idx        = 0;
    song_state_compute_derived(state);
}
