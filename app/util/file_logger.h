#pragma once

/// Installs a custom raylib TraceLog callback that writes to both
/// stdout and a log file. Call before InitWindow().
void file_logger_init(const char* log_path = "shapeshifter.log");

/// Flushes and closes the log file.
void file_logger_shutdown();
