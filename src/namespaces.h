#pragma once

#include <string>
#include <functional>

// ─────────────────────────────────────────────────────────────────────────────
// Namespace Isolation
// ─────────────────────────────────────────────────────────────────────────────

// Creates an isolated child process using clone() with the following namespaces:
//   PID   — container sees itself as PID 1
//   MNT   — mount operations are isolated from host
//   UTS   — hostname is isolated
//   IPC   — inter-process communication isolated
//   NET   — separate network stack (loopback only initially)
//
// The child_func runs inside the new namespace context.
// Returns the child PID (from the parent's perspective).
pid_t create_isolated_process(
    std::function<int()> child_func
);

// Set the hostname inside a UTS namespace
void set_container_hostname(const std::string& hostname);
