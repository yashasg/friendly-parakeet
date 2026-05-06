#pragma once

#include <filesystem>
#include <system_error>

namespace persistence {

enum class Status {
    Success,
    MissingFile,
    CorruptData,
    PathUnavailable,
    DirectoryCreateFailed,
    FileOpenFailed,
    FileReadFailed,
    FileWriteFailed
};

struct Result {
    Status status{Status::Success};
    std::error_code error{};

    [[nodiscard]] bool ok() const {
        return status == Status::Success;
    }
};

struct DirtyPersistenceState {
    std::filesystem::path path;
    Result last_load{};
    Result last_save{};
    bool dirty{false};
};

struct Paths {
    std::filesystem::path root_dir;
    std::filesystem::path settings_file;
    std::filesystem::path high_scores_file;
};

enum class FileKind {
    Settings,
    HighScores
};

Result resolve_paths(Paths& out_paths, const std::filesystem::path& root_override = {});
Result resolve_file_path(std::filesystem::path& out_path,
                         FileKind file_kind,
                         const std::filesystem::path& root_override = {});

const char* status_name(Status status);

}  // namespace persistence
