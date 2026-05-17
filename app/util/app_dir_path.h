#pragma once

#include <string>
#include <string_view>

namespace util {

// True if `p` is an absolute path. Recognizes:
//   - POSIX-rooted: leading '/'
//   - Windows UNC / drive-rooted: leading '\'
//   - Windows drive-letter: ASCII letter followed by ':' (e.g. "C:\foo", "C:/foo")
inline bool is_absolute_path(std::string_view p) noexcept {
    if (p.empty()) return false;
    if (p.front() == '/' || p.front() == '\\') return true;
    if (p.size() >= 2 && p[1] == ':') {
        const char c = p.front();
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }
    return false;
}

// Slash-safe join of raylib's GetApplicationDirectory() (or any prefix) and a
// relative path. Returns app_dir + (sep if missing) + rel.
//
// raylib's GetApplicationDirectory() *typically* returns a trailing separator
// on macOS/Linux/Windows, but the contract is not guaranteed across every
// platform shim (notably some emscripten/sandboxed bundle configurations).
// Concatenating without normalizing the separator can yield a broken path like
// "/path/to/appcontent/fonts/..." which silently falls back to the CWD path.
// Routing through this helper makes that bug class unreachable. See issue
// #1338 for the catalog of formerly-unsafe call sites.
//
// Special cases:
//   - empty app_dir: returns std::string(rel) — caller's relative path wins.
//   - empty rel: returns std::string(app_dir) — no spurious separator appended.
//   - `rel` already absolute: returns std::string(rel) — never prepend app_dir
//     onto an absolute path (issue #1361). Without this guard, callers that
//     pass through user-supplied absolute paths (e.g. PlaySessionContentOverride
//     beatmap_path in tests) produce broken concatenations like
//     "<app_dir>//var/folders/..." that succeed only via fallback retry and
//     emit a spurious WARNING: FILEIO line on every load.
inline std::string join_app_dir(std::string_view app_dir, std::string_view rel) {
    if (app_dir.empty()) return std::string(rel);
    if (rel.empty()) return std::string(app_dir);
    if (is_absolute_path(rel)) return std::string(rel);
    const char last = app_dir.back();
    const bool has_sep = (last == '/' || last == '\\');
    std::string out;
    out.reserve(app_dir.size() + (has_sep ? 0u : 1u) + rel.size());
    out.append(app_dir);
    if (!has_sep) out.push_back('/');
    out.append(rel);
    return out;
}

} // namespace util
