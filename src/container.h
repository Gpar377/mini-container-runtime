#pragma once

#include "cgroups.h"

#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Container State
// ─────────────────────────────────────────────────────────────────────────────

enum class ContainerState {
    CREATED,
    RUNNING,
    STOPPED,
};

const char* state_to_string(ContainerState state);

// ─────────────────────────────────────────────────────────────────────────────
// Container
// ─────────────────────────────────────────────────────────────────────────────

struct Container {
    std::string id;
    pid_t pid = -1;
    ContainerState state = ContainerState::CREATED;
    CgroupLimits limits;

    std::string rootfs_path;
    std::vector<std::string> command;
    std::string hostname;
};

// Create, start, and wait for a container to finish.
// This is the main entry point called by the CLI.
//
// Parameters:
//   rootfs_path — path to the base rootfs directory (e.g., ./rootfs)
//   command     — command + args to execute inside the container
//   limits      — cgroup resource limits
//   hostname    — container hostname (default: "container")
//
// Returns the container's exit code.
int run_container(
    const std::string& rootfs_path,
    const std::vector<std::string>& command,
    const CgroupLimits& limits,
    const std::string& hostname = "container"
);
