#include "beat_map_loader.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <optional>

using json = nlohmann::json;

static bool try_load_constants_from(const std::string& path, ValidationConstants& vc) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    try {
        json j = json::parse(file);
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
    } catch (...) {
        return false;
    }
    return true;
}

ValidationConstants load_validation_constants(const std::string& app_dir) {
    ValidationConstants vc;
    if (!app_dir.empty() && try_load_constants_from(app_dir + "content/constants.json", vc)) {
        return vc;
    }
    try_load_constants_from("content/constants.json", vc);
    return vc;
}

static std::optional<ObstacleKind> parse_kind(const std::string& s) {
    if (s == "shape_gate")       return ObstacleKind::ShapeGate;
    if (s == "lane_block")       return ObstacleKind::LaneBlock;
    if (s == "low_bar")          return ObstacleKind::LowBar;
    if (s == "high_bar")         return ObstacleKind::HighBar;
    if (s == "combo_gate")       return ObstacleKind::ComboGate;
    if (s == "split_path")       return ObstacleKind::SplitPath;
    if (s == "lane_push_left")   return ObstacleKind::LanePushLeft;
    if (s == "lane_push_right")  return ObstacleKind::LanePushRight;
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
    out.song_id    = j.value("song_id", "");
    out.title      = j.value("title", "");
    out.bpm        = j.value("bpm", 120.0f);
    out.offset     = j.value("offset", 0.0f);
    out.lead_beats = j.value("lead_beats", 4);
    out.duration   = j.value("duration_sec", 180.0f);
    out.difficulty = difficulty;

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
        }

        entry.lane = static_cast<int8_t>(b.value("lane", 1));

        if (b.contains("blocked") && b["blocked"].is_array()) {
            entry.blocked_mask = 0;
            for (const auto& lane_idx : b["blocked"]) {
                int l = lane_idx.get<int>();
                if (l >= 0 && l < 3) {
                    entry.blocked_mask |= static_cast<uint8_t>(1 << l);
                }
            }
        }

        out.beats.push_back(entry);
    }

    if (!parse_ok) return false;

    // Sort beats by beat_index (chart may not be pre-sorted)
    std::sort(out.beats.begin(), out.beats.end(),
              [](const BeatEntry& a, const BeatEntry& b) {
                  return a.beat_index < b.beat_index;
              });

    return true;
}

bool load_beat_map(const std::string& json_path, BeatMap& out,
                   std::vector<BeatMapError>& errors,
                   const std::string& difficulty) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        errors.push_back({-1, "Could not open file: " + json_path});
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
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

    for (size_t i = 0; i < map.beats.size(); ++i) {
        const auto& entry = map.beats[i];

        // Rule 1: beat indices monotonically increasing
        if (entry.beat_index <= prev_beat && prev_beat >= 0) {
            errors.push_back({entry.beat_index, "Beat indices must be monotonically increasing"});
            valid = false;
        }
        prev_beat = entry.beat_index;

        // Rule 2: no beat index beyond song duration
        if (entry.beat_index > max_beat) {
            errors.push_back({entry.beat_index, "Beat index exceeds song duration"});
            valid = false;
        }

        // Rule 5: shape_gate / split_path must have lane 0-2
        if (entry.kind == ObstacleKind::ShapeGate || entry.kind == ObstacleKind::SplitPath) {
            if (entry.lane < 0 || entry.lane > 2) {
                errors.push_back({entry.beat_index, "Lane must be 0-2"});
                valid = false;
            }
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

        // Rule 6: different-shape gates must be >= min_shape_change_gap beats apart
        bool has_shape = (entry.kind == ObstacleKind::ShapeGate ||
                          entry.kind == ObstacleKind::ComboGate ||
                          entry.kind == ObstacleKind::SplitPath);
        if (has_shape && prev_had_shape) {
            if (entry.shape != prev_shape &&
                (entry.beat_index - prev_shape_beat) < vc.min_shape_change_gap) {
                errors.push_back({entry.beat_index,
                    "Different-shape gates must be >= " + std::to_string(vc.min_shape_change_gap) + " beats apart"});
                valid = false;
            }
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
