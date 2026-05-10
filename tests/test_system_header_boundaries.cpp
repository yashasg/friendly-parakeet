#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {

std::string read_text(const std::filesystem::path& path) {
    std::ifstream in(path);
    REQUIRE(in.is_open());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::filesystem::path repo_header(const char* rel) {
    const std::filesystem::path local = rel;
    if (std::filesystem::exists(local)) return local;
    return std::filesystem::path("../") / rel;
}

}  // namespace

TEST_CASE("system headers: headless API excludes runtime-only declarations",
          "[systems][headers][issue-406]") {
    const std::string headless =
        read_text(repo_header("app/systems/all_systems.h"));
    const std::string runtime =
        read_text(repo_header("app/systems/runtime_systems.h"));

    CHECK(headless.find("input_system(") == std::string::npos);
    CHECK(headless.find("input_system_init(") == std::string::npos);
    CHECK(headless.find("song_playback_system(") == std::string::npos);
    CHECK(headless.find("game_render_system(") == std::string::npos);
    CHECK(headless.find("ui_render_system(") == std::string::npos);
    CHECK(headless.find("game_camera_system(") == std::string::npos);
    CHECK(headless.find("tick_fixed_systems(") == std::string::npos);

    CHECK(runtime.find("input_system(") != std::string::npos);
    CHECK(runtime.find("song_playback_system(") != std::string::npos);
    CHECK(runtime.find("game_render_system(") != std::string::npos);
    CHECK(runtime.find("ui_render_system(") != std::string::npos);
    CHECK(runtime.find("game_camera_system(") != std::string::npos);
    CHECK(runtime.find("tick_fixed_systems(") != std::string::npos);
}
