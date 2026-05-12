// Source-contract regression checks for #502.
// Native tests cannot execute browser beforeunload events, so we assert the
// shutdown ordering contract directly in source.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace {

std::string read_file(const fs::path& p) {
    std::ifstream f(p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

fs::path find_repo_root() {
    fs::path p = fs::current_path();
    for (int i = 0; i < 8; ++i) {
        if (fs::exists(p / "app" / "game_loop.cpp")) return p;
        if (!p.has_parent_path() || p.parent_path() == p) break;
        p = p.parent_path();
    }
    return fs::current_path();
}

} // namespace

TEST_CASE("wasm lifecycle: shutdown disarms beforeunload before registry reset (#502)",
          "[lifecycle][architecture]") {
    const fs::path root = find_repo_root();
    const std::string game_loop_source = read_file(root / "app" / "game_loop.cpp");

    const std::size_t disarm_pos = game_loop_source.find("platform_disarm_wasm_beforeunload(reg);");
    const std::size_t reset_pos = game_loop_source.find("reg = entt::registry{};");

    REQUIRE(disarm_pos != std::string::npos);
    REQUIRE(reset_pos != std::string::npos);
    CHECK(disarm_pos < reset_pos);
}

TEST_CASE("wasm lifecycle: disarm path unregisters browser callback (#502)",
          "[lifecycle][architecture]") {
    const fs::path root = find_repo_root();
    const std::string platform_source = read_file(root / "app" / "platform_display.cpp");

    const std::size_t disarm_fn = platform_source.find("void platform_disarm_wasm_beforeunload(entt::registry& reg)");
    const std::size_t unregister_call = platform_source.find(
        "emscripten_set_beforeunload_callback(nullptr, nullptr);", disarm_fn);

    REQUIRE(disarm_fn != std::string::npos);
    REQUIRE(unregister_call != std::string::npos);
}

TEST_CASE("wasm lifecycle: visibilitychange callback pauses active play",
          "[lifecycle][architecture]") {
    const fs::path root = find_repo_root();
    const std::string platform_source = read_file(root / "app" / "platform_display.cpp");

    CHECK(platform_source.find("emscripten_set_visibilitychange_callback") != std::string::npos);
    CHECK(platform_source.find("on_visibility_change") != std::string::npos);
    CHECK(platform_source.find("gs->next_phase = GamePhase::Paused;") != std::string::npos);
}

TEST_CASE("wasm smoke phase title markers are compile-gated",
           "[lifecycle][architecture]") {
    const fs::path root = find_repo_root();
    const std::string phase_source = read_file(root / "app" / "systems" / "game_phase_transition.cpp");

    CHECK(phase_source.find("SHAPESHIFTER [") != std::string::npos);
    CHECK(phase_source.find("__EMSCRIPTEN__) && defined(SHAPESHIFTER_WASM_SMOKE_MARKERS)") != std::string::npos);
}

TEST_CASE("wasm persistence uses explicit IDBFS policy and save flush hooks", "[persistence][architecture]") {
    const fs::path root = find_repo_root();
    const std::string cmake_source = read_file(root / "CMakeLists.txt");
    const std::string policy_source = read_file(root / "app" / "util" / "persistence_policy.cpp");
    const std::string settings_source = read_file(root / "app" / "entities" / "settings.cpp");
    const std::string high_score_source = read_file(root / "app" / "util" / "high_score_persistence.cpp");

    CHECK(cmake_source.find("-sFORCE_FILESYSTEM=1") != std::string::npos);
    CHECK(cmake_source.find("-lidbfs.js") != std::string::npos);
    CHECK(policy_source.find("FS.mount(IDBFS") != std::string::npos);
    CHECK(policy_source.find("FS.syncfs") != std::string::npos);
    CHECK(policy_source.find("\"/persistent/shapeshifter\"") != std::string::npos);
    CHECK(settings_source.find("persistence::prepare_for_persistence_read(path)") != std::string::npos);
    CHECK(settings_source.find("persistence::flush_persistence_writes(path)") != std::string::npos);
    CHECK(high_score_source.find("persistence::prepare_for_persistence_read(path)") != std::string::npos);
    CHECK(high_score_source.find("persistence::flush_persistence_writes(path)") != std::string::npos);
}
