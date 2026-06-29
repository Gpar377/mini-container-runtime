#include "network.h"
#include "utils.h"

#include <cstdlib>
#include <stdexcept>

// ─────────────────────────────────────────────────────────────────────────────
// Network Helpers (using ip command — pragmatic for portfolio project)
// ─────────────────────────────────────────────────────────────────────────────

// In production runtimes (runc, containerd), networking is done via netlink
// sockets. For this project, we use the `ip` command for clarity and
// readability — the syscalls are identical under the hood.

static void run_cmd(const std::string& cmd) {
    LOG_DEBUG("exec: " + cmd);
    int ret = system(cmd.c_str());
    if (ret != 0) {
        LOG_WARN("Command returned non-zero: " + cmd);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Host-Side Network Setup
// ─────────────────────────────────────────────────────────────────────────────

void setup_container_network(pid_t container_pid, const std::string& container_id) {
    // Use container_id prefix (first 8 chars) for interface names
    // Interface names must be < 16 chars on Linux
    std::string suffix = container_id.substr(0, 8);
    std::string veth_host = "veth-" + suffix;
    std::string veth_cont = "eth0-" + suffix;

    LOG_INFO("Setting up network for container " + container_id);

    // Step 1: Create veth pair
    // A veth pair is two virtual network interfaces connected like a pipe.
    // Packets sent to one end appear on the other.
    run_cmd("ip link add " + veth_host + " type veth peer name " + veth_cont);

    // Step 2: Move the container-side veth into the container's network namespace
    // After this, only the container can see veth_cont.
    run_cmd("ip link set " + veth_cont + " netns " + std::to_string(container_pid));

    // Step 3: Configure host-side interface
    run_cmd("ip addr add 172.20.0.1/24 dev " + veth_host);
    run_cmd("ip link set " + veth_host + " up");

    LOG_INFO("Host-side veth '" + veth_host + "' configured: 172.20.0.1/24");
}

// ─────────────────────────────────────────────────────────────────────────────
// Container-Side Network Setup (called from inside the container)
// ─────────────────────────────────────────────────────────────────────────────

void setup_container_network_inside() {
    // At this point we're inside the container's network namespace.
    // The veth peer was moved here by the parent and will appear as an
    // interface we need to find and configure.

    // Bring up loopback
    run_cmd("ip link set lo up");

    // Find and configure the container-side veth
    // It was moved into our namespace — find any interface that isn't "lo"
    // For simplicity, we rename whatever interface we find to "eth0"
    run_cmd("ip link set $(ip -o link show | grep -v lo | grep -v sit | head -1 | awk -F: '{print $2}' | tr -d ' ') name eth0 2>/dev/null || true");
    run_cmd("ip addr add 172.20.0.2/24 dev eth0 2>/dev/null || true");
    run_cmd("ip link set eth0 up 2>/dev/null || true");

    // Set default route through host veth
    run_cmd("ip route add default via 172.20.0.1 2>/dev/null || true");

    LOG_INFO("Container network configured: eth0=172.20.0.2/24, gw=172.20.0.1");
}

// ─────────────────────────────────────────────────────────────────────────────
// Cleanup
// ─────────────────────────────────────────────────────────────────────────────

void cleanup_container_network(const std::string& container_id) {
    std::string suffix = container_id.substr(0, 8);
    std::string veth_host = "veth-" + suffix;

    // Deleting one end of a veth pair automatically deletes the other
    run_cmd("ip link del " + veth_host + " 2>/dev/null || true");
    LOG_INFO("Cleaned up network for container " + container_id);
}
