#pragma once

#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Container Networking
// ─────────────────────────────────────────────────────────────────────────────

// Set up container networking:
//   1. Create a veth pair (virtual ethernet) linking host and container
//   2. Move container-side veth into the container's network namespace
//   3. Configure IP addresses on both ends
//   4. Bring up interfaces + loopback
//
// This gives the container its own network stack with connectivity to the host.
void setup_container_network(pid_t container_pid, const std::string& container_id);

// Set up the container-side network interface (called inside the container)
void setup_container_network_inside();

// Clean up host-side veth interface
void cleanup_container_network(const std::string& container_id);
