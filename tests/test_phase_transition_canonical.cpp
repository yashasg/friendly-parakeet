// Enforcement tests for #482/#503:
// direct enter_phase() calls are only allowed from the canonical allow-list
// documented in decisions.md.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct SourceRecord {
    std::string logical_path;
    std::string body;
};

std::string read_file(const fs::path& p) {
    std::ifstream f(p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
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

bool is_type_like_prev_char(char c) {
    const unsigned char uc = static_cast<unsigned char>(c);
    return std::isalnum(uc) || c == '_' || c == '*' || c == '&' || c == '>';
}

bool has_direct_enter_phase_call(const std::string& source) {
    static const std::regex kEnterPhaseToken(R"(\benter_phase\s*\()");
    const std::string sanitized = strip_comments_and_literals(source);

    for (std::sregex_iterator it(sanitized.begin(), sanitized.end(), kEnterPhaseToken), end;
         it != end; ++it) {
        std::size_t pos = static_cast<std::size_t>(it->position());
        while (pos > 0) {
            const char prev = sanitized[pos - 1];
            if (!std::isspace(static_cast<unsigned char>(prev))) break;
            --pos;
        }

        if (pos > 0 && is_type_like_prev_char(sanitized[pos - 1])) {
            continue; // declaration/definition like "void enter_phase(...)"
        }
        return true;
    }

    return false;
}

const std::set<std::string>& canonical_enter_phase_allowlist() {
    static const std::set<std::string> kAllowlist = {
        "app/systems/play_session.cpp",
        "app/systems/game_state_system.cpp",
        "app/systems/game_state_terminal_phase_system.cpp",
    };
    return kAllowlist;
}

std::vector<std::string> collect_enter_phase_offenders(const std::vector<SourceRecord>& sources,
                                                       const std::set<std::string>& allowlist) {
    std::vector<std::string> offenders;
    for (const auto& src : sources) {
        if (!has_direct_enter_phase_call(src.body)) continue;
        if (allowlist.count(src.logical_path) != 0) continue;
        offenders.push_back(src.logical_path);
    }
    std::sort(offenders.begin(), offenders.end());
    return offenders;
}

bool should_skip_source_path(const fs::path& path) {
    for (const auto& part : path) {
        const auto name = part.string();
        if (name == "build" || name == "build-web" || name == "vendor" ||
            name == "vcpkg_installed" || name == "node_modules" ||
            name == "generated") {
            return true;
        }
    }
    return false;
}

std::vector<fs::path> production_app_sources(const fs::path& app_root) {
    std::vector<fs::path> out;
    if (!fs::exists(app_root)) return out;

    for (const auto& entry : fs::recursive_directory_iterator(app_root)) {
        if (!entry.is_regular_file()) continue;
        const auto rel = fs::relative(entry.path(), app_root);
        if (should_skip_source_path(rel)) continue;

        const auto ext = entry.path().extension().string();
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp") {
            out.push_back(entry.path());
        }
    }

    return out;
}

fs::path find_repo_root() {
    fs::path p = fs::current_path();
    for (int i = 0; i < 8; ++i) {
        if (fs::exists(p / "app")) return p;
        if (!p.has_parent_path() || p.parent_path() == p) break;
        p = p.parent_path();
    }
    return fs::current_path();
}

std::vector<SourceRecord> load_app_sources(const fs::path& root) {
    std::vector<SourceRecord> loaded;
    const fs::path app_root = root / "app";
    for (const auto& abs_path : production_app_sources(app_root)) {
        loaded.push_back(SourceRecord{
            fs::relative(abs_path, root).generic_string(),
            read_file(abs_path),
        });
    }
    return loaded;
}

} // namespace

TEST_CASE("phase_transition: app sources enforce canonical enter_phase allow-list (#482, #503)",
          "[phase_transition][architecture]") {
    const fs::path root = find_repo_root();
    REQUIRE(fs::exists(root / "app"));

    const auto sources = load_app_sources(root);
    const auto offenders = collect_enter_phase_offenders(sources, canonical_enter_phase_allowlist());

    INFO("Only canonical callers may invoke enter_phase directly: "
         "app/systems/game_state_system.cpp, app/systems/play_session.cpp, "
         "app/systems/game_state_terminal_phase_system.cpp");
    for (const auto& f : offenders) INFO("offender: " << f);
    REQUIRE(offenders.empty());
}

TEST_CASE("phase_transition: enforcement catches non-UI/non-input direct callers (#503)",
          "[phase_transition][architecture]") {
    const std::vector<SourceRecord> fixtures = {
        {"app/systems/game_state_system.cpp", "void ok(GameState& gs){ enter_phase(gs, GamePhase::Paused); }"},
        {"app/systems/rogue_system.cpp", "void bad(GameState& gs){ enter_phase(gs, GamePhase::Title); }"},
        {"app/ui/screen_controllers/title_screen_controller.cpp", "void ui(GameState& gs){ gs.transition_pending = true; }"},
    };

    const auto offenders = collect_enter_phase_offenders(fixtures, canonical_enter_phase_allowlist());
    REQUIRE(offenders == std::vector<std::string>{"app/systems/rogue_system.cpp"});
}

TEST_CASE("phase_transition: Tutorial and Paused controllers guard entry input",
          "[phase_transition][ui]") {
    const fs::path root = find_repo_root();
    const std::string tutorial =
        read_file(root / "app/ui/screen_controllers/tutorial_screen_controller.cpp");
    const std::string paused =
        read_file(root / "app/ui/screen_controllers/paused_screen_controller.cpp");

    REQUIRE(tutorial.find("constants::UI_ENTRY_DEBOUNCE") != std::string::npos);
    REQUIRE(paused.find("constants::UI_ENTRY_DEBOUNCE") != std::string::npos);
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
    const std::string declaration_only = R"cpp(
        void enter_phase(GameState&, GamePhase);
    )cpp";

    CHECK(has_direct_enter_phase_call(direct_call));
    CHECK_FALSE(has_direct_enter_phase_call(string_literal_only));
    CHECK_FALSE(has_direct_enter_phase_call(comments_only));
    CHECK_FALSE(has_direct_enter_phase_call(declaration_only));
}
