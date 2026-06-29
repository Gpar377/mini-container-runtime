#include "cgroups.h"
#include "utils.h"

#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <cstring>
#include <cerrno>
#include <stdexcept>

// cgroups v2 unified hierarchy base path
static const std::string CGROUP_BASE = "/sys/fs/cgroup/mini-container/";

// ─────────────────────────────────────────────────────────────────────────────
// Cgroup Setup
// ─────────────────────────────────────────────────────────────────────────────

void setup_cgroup(const std::string& container_id, pid_t pid, const CgroupLimits& limits) {
    std::string cgroup_path = CGROUP_BASE + container_id;

    // Create the cgroup directory
    // mkdir -p /sys/fs/cgroup/mini-container/<container_id>/
    ensure_dir(CGROUP_BASE);

    if (mkdir(cgroup_path.c_str(), 0755) != 0 && errno != EEXIST) {
        throw std::runtime_error("Failed to create cgroup dir: " + cgroup_path
                                 + " — " + std::strerror(errno));
    }

    LOG_INFO("Created cgroup: " + cgroup_path);

    // ── Memory Limit ────────────────────────────────────────────────────────
    // memory.max controls the hard memory limit. If the container exceeds
    // this, the kernel OOM-kills its processes.
    write_file(
        cgroup_path + "/memory.max",
        std::to_string(limits.memory_max_bytes)
    );
    LOG_INFO("Set memory.max = " + std::to_string(limits.memory_max_bytes / (1024*1024)) + " MB");

    // Disable swap to make memory limits strict
    // (prevents the container from swapping to bypass the memory limit)
    try {
        write_file(cgroup_path + "/memory.swap.max", "0");
        LOG_INFO("Set memory.swap.max = 0 (swap disabled)");
    } catch (...) {
        LOG_WARN("Could not set memory.swap.max (swap controller may not be available)");
    }

    // ── CPU Limit ───────────────────────────────────────────────────────────
    // cpu.max format: "$QUOTA $PERIOD" (in microseconds)
    // Example: "50000 100000" means 50ms out of every 100ms = 50% of one CPU
    std::string cpu_max = std::to_string(limits.cpu_quota_us) + " "
                        + std::to_string(limits.cpu_period_us);
    write_file(cgroup_path + "/cpu.max", cpu_max);

    double cpu_pct = (static_cast<double>(limits.cpu_quota_us) / limits.cpu_period_us) * 100.0;
    LOG_INFO("Set cpu.max = " + cpu_max + " (" + std::to_string(static_cast<int>(cpu_pct)) + "% of one core)");

    // ── PID Limit ───────────────────────────────────────────────────────────
    // pids.max caps the number of processes. This prevents fork bombs.
    write_file(cgroup_path + "/pids.max", std::to_string(limits.pids_max));
    LOG_INFO("Set pids.max = " + std::to_string(limits.pids_max));

    // ── Assign Container Process to Cgroup ──────────────────────────────────
    // Writing the PID to cgroup.procs moves that process (and all its
    // children) into this cgroup.
    write_file(cgroup_path + "/cgroup.procs", std::to_string(pid));
    LOG_INFO("Added PID " + std::to_string(pid) + " to cgroup");
}

// ─────────────────────────────────────────────────────────────────────────────
// Cgroup Cleanup
// ─────────────────────────────────────────────────────────────────────────────

void cleanup_cgroup(const std::string& container_id) {
    std::string cgroup_path = CGROUP_BASE + container_id;

    // Removing a cgroup directory requires that:
    // 1. No processes are running in it (they must have exited)
    // 2. No child cgroups exist
    // rmdir() on the cgroup directory tells the kernel to release it
    if (rmdir(cgroup_path.c_str()) == 0) {
        LOG_INFO("Removed cgroup: " + cgroup_path);
    } else {
        LOG_WARN("Could not remove cgroup " + cgroup_path + ": " + std::strerror(errno));
    }
}
