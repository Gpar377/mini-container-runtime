#pragma once

#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Filesystem Sandboxing
// ─────────────────────────────────────────────────────────────────────────────

// Set up the container's isolated filesystem:
//   1. Create OverlayFS mount (read-only rootfs + read-write upper layer)
//   2. Mount virtual filesystems (/proc, /sys, /dev)
//   3. pivot_root into the merged directory
//   4. Unmount and remove the old root
//
// After this call, the process is fully confined to the container's filesystem.
//
// Parameters:
//   rootfs_path    — path to the base rootfs (e.g., Alpine minirootfs)
//   container_id   — unique container identifier for per-container overlay dirs
void setup_container_filesystem(const std::string& rootfs_path, const std::string& container_id);

// Clean up container filesystem artifacts (overlay dirs)
void cleanup_container_filesystem(const std::string& container_id);
