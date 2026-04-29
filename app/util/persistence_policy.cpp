#include "persistence_policy.h"

#include "fs_utils.h"

#include <cstdlib>

#ifndef _WIN32
    #include <pwd.h>
    #include <unistd.h>
#endif

#if defined(__APPLE__)
    #include <TargetConditionals.h>
#endif

namespace persistence {
namespace {

std::filesystem::path resolve_root_dir(const std::filesystem::path& root_override) {
    if (!root_override.empty()) {
        return root_override;
    }

#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata) return std::filesystem::path(appdata) / "shapeshifter";
    return {};
#elif defined(__EMSCRIPTEN__)
    return std::filesystem::path(".");
#else
    const char* home = std::getenv("HOME");
    if (!home) {
        const passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home) return {};

#if defined(__APPLE__) && defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    return std::filesystem::path(home) / "Library" / "Application Support" / "shapeshifter";
#else
    return std::filesystem::path(home) / ".shapeshifter";
#endif
#endif
}

}  // namespace

Result resolve_paths(Paths& out_paths, const std::filesystem::path& root_override) {
    const auto root_dir = resolve_root_dir(root_override);
    if (root_dir.empty()) {
        return Result{Status::PathUnavailable, {}};
    }

    const auto ensure = fs_utils::ensure_directory_result(root_dir);
    if (!ensure.ok) {
        return Result{Status::DirectoryCreateFailed, ensure.error};
    }

    out_paths.root_dir = root_dir;
    out_paths.settings_file = root_dir / "settings.json";
    out_paths.high_scores_file = root_dir / "high_scores.json";
    return Result{};
}

const char* status_name(const Status status) {
    switch (status) {
        case Status::Success: return "success";
        case Status::MissingFile: return "missing_file";
        case Status::CorruptData: return "corrupt_data";
        case Status::PathUnavailable: return "path_unavailable";
        case Status::DirectoryCreateFailed: return "directory_create_failed";
        case Status::FileOpenFailed: return "file_open_failed";
        case Status::FileReadFailed: return "file_read_failed";
        case Status::FileWriteFailed: return "file_write_failed";
    }
    return "unknown";
}

}  // namespace persistence
