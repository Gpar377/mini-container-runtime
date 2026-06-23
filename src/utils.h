#pragma once

#include <string>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
// Logging
// ─────────────────────────────────────────────────────────────────────────────

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

void log(LogLevel level, const std::string& msg);

// Convenience macros
#define LOG_DEBUG(msg) log(LogLevel::DEBUG, msg)
#define LOG_INFO(msg)  log(LogLevel::INFO,  msg)
#define LOG_WARN(msg)  log(LogLevel::WARN,  msg)
#define LOG_ERROR(msg) log(LogLevel::ERROR, msg)

// ─────────────────────────────────────────────────────────────────────────────
// Syscall Safety
// ─────────────────────────────────────────────────────────────────────────────

// Wraps a syscall return value. Throws std::runtime_error on failure with
// errno-aware message. Usage: syscall_check(mount(...), "mount /proc");
void syscall_check(int ret, const std::string& context);

// ─────────────────────────────────────────────────────────────────────────────
// Filesystem Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Write a string to a file (used for cgroup configuration)
void write_file(const std::string& path, const std::string& content);

// Read entire file contents into a string
std::string read_file(const std::string& path);

// Ensure a directory exists (mkdir -p equivalent)
void ensure_dir(const std::string& path);

// Generate a random 12-character hex container ID
std::string generate_container_id();
