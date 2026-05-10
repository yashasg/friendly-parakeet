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

TEST_CASE("component headers avoid raylib backend types", "[components][boundary][issue-407]") {
    const std::string transform = read_text(repo_header("app/components/transform.h"));
    const std::string rendering = read_text(repo_header("app/components/rendering.h"));
    const std::string text = read_text(repo_header("app/components/text.h"));

    CHECK(transform.find("#include <raylib.h>") == std::string::npos);
    CHECK(rendering.find("#include <raylib.h>") == std::string::npos);
    CHECK(text.find("#include <raylib.h>") == std::string::npos);

    CHECK(transform.find("Vector2") == std::string::npos);
    CHECK(rendering.find("Matrix") == std::string::npos);
    CHECK(rendering.find("TintColor") != std::string::npos);
}
