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
        if (body.find("enter_phase(") != std::string::npos) {
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
        if (body.find("enter_phase(") != std::string::npos) {
            offenders.push_back(src.string());
        }
    }

    INFO("Input routing must signal intent via transition_pending/next_phase, "
         "not call enter_phase() directly. See decisions.md (#482).");
    for (const auto& f : offenders) INFO("offender: " << f);
    REQUIRE(offenders.empty());
}
