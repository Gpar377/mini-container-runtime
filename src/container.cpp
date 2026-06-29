#include "container.h"
#include "namespaces.h"
#include "filesystem.h"
#include "cgroups.h"
#include "network.h"
#include "utils.h"

#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <iostream>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// State helpers
// ─────────────────────────────────────────────────────────────────────────────

const char* state_to_string(ContainerState state) {
    switch (state) {
        case ContainerState::CREATED: return "CREATED";
        case ContainerState::RUNNING: return "RUNNING";
        case ContainerState::STOPPED: return "STOPPED";
    }
    return "UNKNOWN";
}

// ─────────────────────────────────────────────────────────────────────────────
// Container Entry Point (runs inside the namespace-isolated child process)
// ─────────────────────────────────────────────────────────────────────────────

static int container_entry(Container& container) {
    // At this point we are inside the new namespaces (PID, MNT, UTS, IPC, NET)
    // but we haven't yet set up the filesystem or hostname.

    try {
        // Step 1: Set hostname
        set_container_hostname(container.hostname);

        // Step 2: Set up container-side networking (loopback + eth0)
        setup_container_network_inside();

        // Step 3: Set up isolated filesystem (OverlayFS + pivot_root)
        // After this call, we are chrooted into the container's rootfs.
        setup_container_filesystem(container.rootfs_path, container.id);

        // Step 4: Execute the requested command
        // Convert std::vector<std::string> to C-style argv
        std::vector<char*> argv;
        for (auto& arg : container.command) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        LOG_INFO("Executing: " + container.command[0]);

        // execvp replaces this process with the target command.
        // If it returns, it means it failed.
        execvp(argv[0], argv.data());

        // If we get here, execvp failed
        std::cerr << "execvp failed: " << std::strerror(errno) << std::endl;
        return 1;

    } catch (const std::exception& e) {
        std::cerr << "Container setup failed: " << e.what() << std::endl;
        return 1;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Run Container (full lifecycle)
// ─────────────────────────────────────────────────────────────────────────────

int run_container(
    const std::string& rootfs_path,
    const std::vector<std::string>& command,
    const CgroupLimits& limits,
    const std::string& hostname
) {
    Container container;
    container.id = generate_container_id();
    container.rootfs_path = rootfs_path;
    container.command = command;
    container.hostname = hostname;
    container.limits = limits;
    container.state = ContainerState::CREATED;

    LOG_INFO("Container " + container.id + " created");
    LOG_INFO("  Rootfs:   " + rootfs_path);
    LOG_INFO("  Command:  " + command[0]);
    LOG_INFO("  Hostname: " + hostname);
    LOG_INFO("  Memory:   " + std::to_string(limits.memory_max_bytes / (1024*1024)) + " MB");
    LOG_INFO("  CPU:      " + std::to_string(limits.cpu_quota_us) + "/" + std::to_string(limits.cpu_period_us) + " us");
    LOG_INFO("  PIDs max: " + std::to_string(limits.pids_max));

    // ── Create isolated child process ────────────────────────────────────────
    // clone() creates a new process in new namespaces.
    // The child runs container_entry() which sets up FS, hostname, and execs.
    pid_t child_pid = create_isolated_process([&container]() -> int {
        return container_entry(container);
    });

    container.pid = child_pid;
    container.state = ContainerState::RUNNING;

    // ── Parent: set up cgroups and host-side networking ───────────────────────
    // These must happen AFTER clone() returns the child PID because:
    //   - cgroups need the PID to assign the process
    //   - networking needs the PID to move veth into the namespace
    try {
        setup_cgroup(container.id, child_pid, limits);
    } catch (const std::exception& e) {
        LOG_WARN(std::string("Cgroup setup failed (non-fatal): ") + e.what());
    }

    try {
        setup_container_network(child_pid, container.id);
    } catch (const std::exception& e) {
        LOG_WARN(std::string("Network setup failed (non-fatal): ") + e.what());
    }

    LOG_INFO("Container " + container.id + " running (PID " + std::to_string(child_pid) + ")");

    // ── Wait for container to exit ───────────────────────────────────────────
    int status = 0;
    waitpid(child_pid, &status, 0);

    int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    container.state = ContainerState::STOPPED;

    LOG_INFO("Container " + container.id + " exited with code " + std::to_string(exit_code));

    // ── Cleanup ──────────────────────────────────────────────────────────────
    cleanup_cgroup(container.id);
    cleanup_container_network(container.id);
    cleanup_container_filesystem(container.id);

    LOG_INFO("Container " + container.id + " cleaned up");

    return exit_code;
}
