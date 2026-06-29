#include "filesystem.h"
#include "utils.h"

#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

// pivot_root is not exposed in glibc headers, use syscall directly
static int pivot_root(const char* new_root, const char* put_old) {
    return syscall(SYS_pivot_root, new_root, put_old);
}

// Base directory for container overlay layers
static const std::string CONTAINERS_DIR = "./containers/";

// ─────────────────────────────────────────────────────────────────────────────
// OverlayFS Setup
// ─────────────────────────────────────────────────────────────────────────────

static void setup_overlayfs(const std::string& rootfs, const std::string& container_id) {
    std::string base = CONTAINERS_DIR + container_id;
    std::string upper  = base + "/upper";
    std::string work   = base + "/work";
    std::string merged = base + "/merged";

    // Create per-container directories
    ensure_dir(upper);
    ensure_dir(work);
    ensure_dir(merged);

    // Mount OverlayFS: read-only lower (rootfs) + read-write upper
    // This is exactly what Docker does for container layers.
    std::string opts = "lowerdir=" + rootfs
                     + ",upperdir=" + upper
                     + ",workdir=" + work;

    LOG_INFO("Mounting OverlayFS: " + opts);

    syscall_check(
        mount("overlay", merged.c_str(), "overlay", 0, opts.c_str()),
        "mount overlay on " + merged
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// Virtual Filesystem Mounts
// ─────────────────────────────────────────────────────────────────────────────

static void mount_virtual_filesystems(const std::string& merged) {
    // Mount /proc — required for ps, top, /proc/self, etc.
    std::string proc_path = merged + "/proc";
    ensure_dir(proc_path);
    syscall_check(
        mount("proc", proc_path.c_str(), "proc", 0, ""),
        "mount /proc"
    );
    LOG_INFO("Mounted /proc");

    // Mount /sys — required for device/cgroup visibility
    std::string sys_path = merged + "/sys";
    ensure_dir(sys_path);
    syscall_check(
        mount("sysfs", sys_path.c_str(), "sysfs", MS_RDONLY, ""),
        "mount /sys"
    );
    LOG_INFO("Mounted /sys (read-only)");

    // Mount /dev as tmpfs with minimal device nodes
    std::string dev_path = merged + "/dev";
    ensure_dir(dev_path);
    syscall_check(
        mount("tmpfs", dev_path.c_str(), "tmpfs", MS_NOSUID | MS_STRICTATIME, "mode=755"),
        "mount /dev"
    );

    // Create essential device nodes
    // /dev/null, /dev/zero, /dev/random, /dev/urandom
    syscall_check(mknod((dev_path + "/null").c_str(),    S_IFCHR | 0666, makedev(1, 3)), "mknod /dev/null");
    syscall_check(mknod((dev_path + "/zero").c_str(),    S_IFCHR | 0666, makedev(1, 5)), "mknod /dev/zero");
    syscall_check(mknod((dev_path + "/random").c_str(),  S_IFCHR | 0666, makedev(1, 8)), "mknod /dev/random");
    syscall_check(mknod((dev_path + "/urandom").c_str(), S_IFCHR | 0666, makedev(1, 9)), "mknod /dev/urandom");
    LOG_INFO("Mounted /dev with essential device nodes");
}

// ─────────────────────────────────────────────────────────────────────────────
// pivot_root — Replace the Root Filesystem
// ─────────────────────────────────────────────────────────────────────────────

static void do_pivot_root(const std::string& merged) {
    // pivot_root requires that new_root is a mount point.
    // We achieve this by bind-mounting merged onto itself.
    syscall_check(
        mount(merged.c_str(), merged.c_str(), "", MS_BIND | MS_REC, ""),
        "bind mount merged"
    );

    // Create a directory to stash the old root
    std::string old_root = merged + "/old_root";
    ensure_dir(old_root);

    // pivot_root(new_root, put_old):
    //   - Moves the current root to put_old
    //   - Makes new_root the new /
    // This is more secure than chroot because it actually swaps the mount.
    LOG_INFO("Calling pivot_root...");
    syscall_check(
        pivot_root(merged.c_str(), old_root.c_str()),
        "pivot_root"
    );

    // Change working directory to new root
    syscall_check(chdir("/"), "chdir /");

    // Unmount the old root — the container can no longer see the host filesystem
    syscall_check(
        umount2("/old_root", MNT_DETACH),
        "umount old_root"
    );

    // Remove the old_root directory
    rmdir("/old_root");

    LOG_INFO("pivot_root complete — container filesystem isolated");
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void setup_container_filesystem(const std::string& rootfs_path, const std::string& container_id) {
    std::string merged = CONTAINERS_DIR + container_id + "/merged";

    // Step 1: Mount OverlayFS
    setup_overlayfs(rootfs_path, container_id);

    // Step 2: Mount virtual filesystems inside the merged root
    mount_virtual_filesystems(merged);

    // Step 3: pivot_root into the merged directory
    do_pivot_root(merged);
}

void cleanup_container_filesystem(const std::string& container_id) {
    std::string base = CONTAINERS_DIR + container_id;

    // Best-effort cleanup — ignore errors since the container might have
    // already unmounted things or the process might be dead
    std::string merged = base + "/merged";
    umount2(merged.c_str(), MNT_DETACH);

    // Remove container directories
    // In production we'd do a recursive rmdir, but for this project
    // we'll leave cleanup to the OS or manual rm -rf
    LOG_INFO("Cleaned up filesystem for container " + container_id);
}
