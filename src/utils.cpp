#include "utils.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <random>
#include <chrono>
#include <cstring>
#include <cerrno>
#include <stdexcept>

#include <sys/stat.h>

// ─────────────────────────────────────────────────────────────────────────────
// Logging
// ─────────────────────────────────────────────────────────────────────────────

static const char* level_str(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "\033[90m[DEBUG]\033[0m";
        case LogLevel::INFO:  return "\033[32m[INFO]\033[0m ";
        case LogLevel::WARN:  return "\033[33m[WARN]\033[0m ";
        case LogLevel::ERROR: return "\033[31m[ERROR]\033[0m";
    }
    return "[?]";
}

void log(LogLevel level, const std::string& msg) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    char time_buf[32];
    std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", std::localtime(&time_t_now));

    std::cerr << time_buf << "." << std::setfill('0') << std::setw(3) << ms.count()
              << " " << level_str(level) << " " << msg << std::endl;
}

// ─────────────────────────────────────────────────────────────────────────────
// Syscall Safety
// ─────────────────────────────────────────────────────────────────────────────

void syscall_check(int ret, const std::string& context) {
    if (ret == -1) {
        std::string err = context + " failed: " + std::strerror(errno);
        LOG_ERROR(err);
        throw std::runtime_error(err);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Filesystem Helpers
// ─────────────────────────────────────────────────────────────────────────────

void write_file(const std::string& path, const std::string& content) {
    std::ofstream ofs(path);
    if (!ofs) {
        throw std::runtime_error("Failed to open for writing: " + path);
    }
    ofs << content;
    ofs.close();
    if (ofs.fail()) {
        throw std::runtime_error("Failed to write to: " + path);
    }
}

std::string read_file(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) {
        throw std::runtime_error("Failed to open for reading: " + path);
    }
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

void ensure_dir(const std::string& path) {
    // Simple recursive mkdir -p implementation
    std::string current;
    for (size_t i = 0; i < path.size(); ++i) {
        current += path[i];
        if (path[i] == '/' || i == path.size() - 1) {
            mkdir(current.c_str(), 0755);  // Ignore EEXIST
        }
    }
}

std::string generate_container_id() {
    static std::mt19937 gen(
        std::chrono::steady_clock::now().time_since_epoch().count()
    );
    static std::uniform_int_distribution<> dist(0, 15);

    const char hex_chars[] = "0123456789abcdef";
    std::string id;
    id.reserve(12);
    for (int i = 0; i < 12; ++i) {
        id += hex_chars[dist(gen)];
    }
    return id;
}
