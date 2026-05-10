// Enforcement test for #482: UI screen controllers and input routing must not
// call enter_phase() directly. They signal intent via transition_pending /
// next_phase and let game_state_system perform the canonical swap. See
// decisions.md → "Phase Transition Mechanism — Single Canonical Path".
//
// This is a build-time grep: scans the controller and input-routing source
// directories and fails if any TU calls `enter_phase(`.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string read_file(const fs::path& p) {
    std::ifstream f(p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::vector<fs::path> sources_under(const fs::path& root) {
    std::vector<fs::path> out;
    if (!fs::exists(root)) return out;
    for (const auto& entry : fs::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) continue;
        const auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp") {
            out.push_back(entry.path());
        }
    }
    return out;
}

std::string strip_comments_and_literals(const std::string& source) {
    std::string sanitized;
    sanitized.reserve(source.size());

    for (std::size_t i = 0; i < source.size();) {
        if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '/') {
            sanitized.push_back(' ');
            sanitized.push_back(' ');
            i += 2;
            while (i < source.size() && source[i] != '\n') {
                sanitized.push_back(' ');
                ++i;
            }
            continue;
        }

        if (i + 1 < source.size() && source[i] == '/' && source[i + 1] == '*') {
            sanitized.push_back(' ');
            sanitized.push_back(' ');
            i += 2;
            while (i < source.size()) {
                if (i + 1 < source.size() && source[i] == '*' && source[i + 1] == '/') {
                    sanitized.push_back(' ');
                    sanitized.push_back(' ');
                    i += 2;
                    break;
                }
                sanitized.push_back(source[i] == '\n' ? '\n' : ' ');
                ++i;
            }
            continue;
        }

        if (source[i] == '"') {
            sanitized.push_back(' ');
            ++i;
            while (i < source.size()) {
                const char c = source[i];
                sanitized.push_back(c == '\n' ? '\n' : ' ');
                ++i;
                if (c == '\\' && i < source.size()) {
                    sanitized.push_back(source[i] == '\n' ? '\n' : ' ');
                    ++i;
                    continue;
                }
                if (c == '"') break;
            }
            continue;
        }

        if (source[i] == '\'') {
            sanitized.push_back(' ');
            ++i;
            while (i < source.size()) {
                const char c = source[i];
                sanitized.push_back(c == '\n' ? '\n' : ' ');
                ++i;
                if (c == '\\' && i < source.size()) {
                    sanitized.push_back(source[i] == '\n' ? '\n' : ' ');
                    ++i;
                    continue;
                }
                if (c == '\'') break;
            }
            continue;
        }

        if (source[i] == 'R' && i + 1 < source.size() && source[i + 1] == '"') {
            const std::size_t delimiter_start = i + 2;
            const std::size_t open_paren = source.find('(', delimiter_start);
            if (open_paren != std::string::npos) {
                const std::string delimiter = source.substr(delimiter_start, open_paren - delimiter_start);
                bool delimiter_valid = true;
                for (const char dc : delimiter) {
                    if (dc == '\\' || dc == ')' || dc == '(' || dc == ' ' || dc == '\t' || dc == '\n' || dc == '\r') {
                        delimiter_valid = false;
                        break;
                    }
                }
                if (delimiter_valid) {
                    const std::string end_marker = ")" + delimiter + "\"";
                    const std::size_t close_pos = source.find(end_marker, open_paren + 1);
                    const std::size_t raw_end = (close_pos == std::string::npos) ? source.size() : close_pos + end_marker.size();
                    while (i < raw_end && i < source.size()) {
                        sanitized.push_back(source[i] == '\n' ? '\n' : ' ');
                        ++i;
                    }
                    continue;
                }
            }
        }

        sanitized.push_back(source[i]);
        ++i;
    }

    return sanitized;
}

bool has_direct_enter_phase_call(const std::string& source) {
    static const std::regex kEnterPhaseCallPattern(R"(\benter_phase\s*\()");
    return std::regex_search(strip_comments_and_literals(source), kEnterPhaseCallPattern);
}

// Walk upward from CWD to locate the source root (must contain the app/
// directory). The test runner may be invoked from build/, build-web/, etc.
fs::path find_repo_root() {
    fs::path p = fs::current_path();
    for (int i = 0; i < 8; ++i) {
        if (fs::exists(p / "app" / "ui" / "screen_controllers")) return p;
        if (!p.has_parent_path() || p.parent_path() == p) break;
        p = p.parent_path();
    }
    return fs::current_path();
}

} // namespace

TEST_CASE("phase_transition: screen controllers do not call enter_phase directly (#482)",
          "[phase_transition][architecture]") {
    const fs::path root = find_repo_root();
    const fs::path controllers = root / "app" / "ui" / "screen_controllers";

    REQUIRE(fs::exists(controllers));

    std::vector<std::string> offenders;
    for (const auto& src : sources_under(controllers)) {
        const std::string body = read_file(src);
        if (has_direct_enter_phase_call(body)) {
            offenders.push_back(src.string());
        }
    }

    INFO("Screen controllers must signal intent via transition_pending/next_phase, "
         "not call enter_phase() directly. See decisions.md (#482).");
    for (const auto& f : offenders) INFO("offender: " << f);
    REQUIRE(offenders.empty());
}

TEST_CASE("phase_transition: input routing does not call enter_phase directly (#482)",
          "[phase_transition][architecture]") {
    const fs::path root = find_repo_root();
    const fs::path input_dir = root / "app" / "input";

    REQUIRE(fs::exists(input_dir));

    std::vector<std::string> offenders;
    for (const auto& src : sources_under(input_dir)) {
        const std::string body = read_file(src);
        if (has_direct_enter_phase_call(body)) {
            offenders.push_back(src.string());
        }
    }

    INFO("Input routing must signal intent via transition_pending/next_phase, "
         "not call enter_phase() directly. See decisions.md (#482).");
    for (const auto& f : offenders) INFO("offender: " << f);
    REQUIRE(offenders.empty());
}

TEST_CASE("phase_transition matcher catches whitespace variants and ignores strings/comments",
          "[phase_transition][architecture]") {
    const std::string direct_call = R"cpp(
        void bad(GameState& gs) {
            enter_phase ( gs, GamePhase::Title );
        }
    )cpp";
    const std::string string_literal_only = R"cpp(
        const char* text = "enter_phase( should not trigger";
    )cpp";
    const std::string comments_only = R"cpp(
        // enter_phase(gs, GamePhase::Title);
        /*
          enter_phase ( gs, GamePhase::Title );
        */
    )cpp";

    CHECK(has_direct_enter_phase_call(direct_call));
    CHECK_FALSE(has_direct_enter_phase_call(string_literal_only));
    CHECK_FALSE(has_direct_enter_phase_call(comments_only));
}
