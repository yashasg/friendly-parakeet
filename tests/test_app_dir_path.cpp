#include <catch2/catch_test_macros.hpp>

#include "util/app_dir_path.h"

// Issue #1338: raylib's GetApplicationDirectory() *typically* returns a
// trailing separator on macOS/Linux/Windows, but the contract is not
// guaranteed across every platform shim. util::join_app_dir is the
// slash-safe replacement for `std::string(app_dir) + rel`.

TEST_CASE("join_app_dir: empty app_dir returns rel verbatim", "[util][app_dir_path][issue1338]") {
    CHECK(util::join_app_dir("", "content/fonts/x.ttf") == "content/fonts/x.ttf");
    CHECK(util::join_app_dir("", "") == "");
}

TEST_CASE("join_app_dir: empty rel returns app_dir verbatim (no spurious sep)", "[util][app_dir_path][issue1338]") {
    CHECK(util::join_app_dir("/path/to/app/", "") == "/path/to/app/");
    CHECK(util::join_app_dir("/path/to/app", "") == "/path/to/app");
}

TEST_CASE("join_app_dir: app_dir already ending in '/' is not double-slashed", "[util][app_dir_path][issue1338]") {
    CHECK(util::join_app_dir("/path/to/app/", "content/x.ttf") == "/path/to/app/content/x.ttf");
}

TEST_CASE("join_app_dir: app_dir ending in '\\\\' (Windows) is not double-slashed", "[util][app_dir_path][issue1338]") {
    CHECK(util::join_app_dir("C:\\path\\to\\app\\", "content/x.ttf") == "C:\\path\\to\\app\\content/x.ttf");
}

TEST_CASE("join_app_dir: app_dir missing separator gets '/' inserted", "[util][app_dir_path][issue1338]") {
    // This is the buggy raylib edge case the helper exists to neutralize:
    // raw concat would yield "/path/to/appcontent/x.ttf" (broken).
    CHECK(util::join_app_dir("/path/to/app", "content/x.ttf") == "/path/to/app/content/x.ttf");
}
