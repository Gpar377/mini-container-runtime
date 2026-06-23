#pragma once

#include <string>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Cgroups v2 Resource Limits
// ─────────────────────────────────────────────────────────────────────────────

struct CgroupLimits {
    int64_t memory_max_bytes = 256 * 1024 * 1024;  // 256 MB default
    int cpu_quota_us         = 100000;               // 100ms (100% of one core)
    int cpu_period_us        = 100000;               // 100ms period
    int pids_max             = 64;                   // Max 64 processes
};

// Create a cgroup for the container and apply resource limits.
// Creates: /sys/fs/cgroup/mini-container/<container_id>/
// Writes limits to memory.max, cpu.max, pids.max
// Writes the container PID to cgroup.procs
void setup_cgroup(const std::string& container_id, pid_t pid, const CgroupLimits& limits);

// Remove the container's cgroup directory
void cleanup_cgroup(const std::string& container_id);
