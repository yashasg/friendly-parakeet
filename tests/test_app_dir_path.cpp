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

// Issue #1361: when the caller passes an already-absolute path as `rel`
// (e.g. PlaySessionContentOverride beatmap_path pointing at /var/folders/...
// in tests), the old helper would produce "<app_dir>//var/folders/..." and
// open() would silently fail before the caller's fallback retry succeeded —
// leaving a spurious WARNING: FILEIO line in every run.

TEST_CASE("join_app_dir: absolute POSIX rel passes through unchanged", "[util][app_dir_path][issue1361]") {
    CHECK(util::join_app_dir("/path/to/app/", "/var/folders/06/tmp.json") == "/var/folders/06/tmp.json");
    CHECK(util::join_app_dir("/path/to/app", "/var/folders/06/tmp.json") == "/var/folders/06/tmp.json");
}

TEST_CASE("join_app_dir: absolute Windows drive-letter rel passes through unchanged", "[util][app_dir_path][issue1361]") {
    CHECK(util::join_app_dir("C:\\path\\to\\app\\", "D:\\temp\\beatmap.json") == "D:\\temp\\beatmap.json");
    CHECK(util::join_app_dir("C:\\path\\to\\app\\", "c:/temp/beatmap.json") == "c:/temp/beatmap.json");
}

TEST_CASE("join_app_dir: absolute Windows UNC rel passes through unchanged", "[util][app_dir_path][issue1361]") {
    CHECK(util::join_app_dir("C:\\path\\to\\app\\", "\\\\server\\share\\file.json") == "\\\\server\\share\\file.json");
}

TEST_CASE("is_absolute_path: recognizes POSIX, Windows UNC, and drive-letter paths", "[util][app_dir_path][issue1361]") {
    CHECK(util::is_absolute_path("/var/folders/x.json"));
    CHECK(util::is_absolute_path("\\\\server\\share"));
    CHECK(util::is_absolute_path("C:\\foo"));
    CHECK(util::is_absolute_path("c:/foo"));
    CHECK_FALSE(util::is_absolute_path(""));
    CHECK_FALSE(util::is_absolute_path("content/x.ttf"));
    CHECK_FALSE(util::is_absolute_path("9:not-a-drive"));
    CHECK_FALSE(util::is_absolute_path("AB:not-a-drive")); // two letters then colon is not a drive
}
