#pragma once

#include <string>
#include <string_view>

namespace util {

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
inline std::string join_app_dir(std::string_view app_dir, std::string_view rel) {
    if (app_dir.empty()) return std::string(rel);
    if (rel.empty()) return std::string(app_dir);
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
