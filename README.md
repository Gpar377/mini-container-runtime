# mini-container

A Linux container runtime built from scratch in C++. No Docker, no containerd, no runc — just raw kernel syscalls.

Creates fully isolated containers using the same primitives Docker uses under the hood: Linux namespaces, cgroups v2, OverlayFS, and `pivot_root`.

```
$ sudo ./mini-container run --memory 128 --cpus 0.5 --pids 32 ./rootfs /bin/sh

  ┌─────────────────────────────────────────────────────────────┐
  │                  mini-container starting                    │
  └─────────────────────────────────────────────────────────────┘

10:42:15.123 [INFO]  Container a3f8c912e4b1 created
10:42:15.124 [INFO]  Creating isolated process with namespaces: PID|MNT|UTS|IPC|NET
10:42:15.126 [INFO]  Hostname set to: container
10:42:15.128 [INFO]  Mounting OverlayFS...
10:42:15.130 [INFO]  pivot_root complete — container filesystem isolated
10:42:15.131 [INFO]  Executing: /bin/sh

/ # hostname
container
/ # echo $$
1
/ # cat /proc/self/cgroup
0::/mini-container/a3f8c912e4b1
```

---

## What It Does

| Feature | Implementation | Docker Equivalent |
|---|---|---|
| **Process Isolation** | `clone()` with `CLONE_NEWPID \| CLONE_NEWNS \| CLONE_NEWUTS \| CLONE_NEWIPC \| CLONE_NEWNET` | `runc create` |
| **Filesystem Sandboxing** | OverlayFS mount + `pivot_root()` | Union mount + `pivot_root` |
| **Resource Limits** | cgroups v2 (`memory.max`, `cpu.max`, `pids.max`) | cgroup driver |
| **Networking** | veth pair + IP assignment | CNI plugins |
| **Device Isolation** | Minimal `/dev` with `mknod` (null, zero, random, urandom) | Device allowlist |

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  CLI (main.cpp)                                                 │
│    Parses args → calls run_container()                          │
├─────────────────────────────────────────────────────────────────┤
│  Container Lifecycle (container.cpp)                            │
│    create → start → wait → cleanup                              │
├──────────┬──────────┬──────────────┬────────────────────────────┤
│ Namespaces│ Filesystem│   Cgroups    │      Network              │
│ clone()   │ OverlayFS │ memory.max   │ veth pair                 │
│ PID/MNT/  │ pivot_root│ cpu.max      │ IP config                 │
│ UTS/IPC/  │ /proc     │ pids.max     │ loopback                  │
│ NET       │ /sys /dev │ swap disable │ default route              │
└──────────┴──────────┴──────────────┴────────────────────────────┘
                         Linux Kernel
```

---

## Build & Run

### Prerequisites
- Linux (or WSL2 on Windows)
- CMake 3.16+, g++ with C++17 support
- Root privileges (for namespace/cgroup syscalls)

### Build
```bash
mkdir build && cd build
cmake ..
make
```

### Download a rootfs
```bash
# Alpine Linux minirootfs (~3MB)
mkdir -p rootfs
wget https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/x86_64/alpine-minirootfs-3.20.0-x86_64.tar.gz
tar -xzf alpine-minirootfs-3.20.0-x86_64.tar.gz -C rootfs
rm alpine-minirootfs-3.20.0-x86_64.tar.gz
```

### Run a container
```bash
# Interactive shell
sudo ./build/mini-container run ./rootfs /bin/sh

# With resource limits
sudo ./build/mini-container run --memory 128 --cpus 0.5 --pids 32 ./rootfs /bin/sh

# Run a command directly
sudo ./build/mini-container run ./rootfs /bin/echo "Hello from container!"

# Custom hostname
sudo ./build/mini-container run --hostname my-box ./rootfs /bin/hostname
```

### Run tests
```bash
sudo bash tests/test_container.sh
```

---

## How It Works

### 1. Namespace Isolation (`namespaces.cpp`)
Uses the `clone()` syscall with five namespace flags to create a child process in a completely isolated environment:
- **PID**: Container sees itself as PID 1
- **MNT**: Mount operations don't affect the host
- **UTS**: Hostname is isolated
- **IPC**: Shared memory/semaphores are isolated
- **NET**: Separate network stack

### 2. Filesystem Sandboxing (`filesystem.cpp`)
Three-step process to confine the container:
1. **OverlayFS**: Layers a read-write directory on top of the read-only rootfs (same copy-on-write mechanism Docker uses for image layers)
2. **Virtual FS**: Mounts `/proc`, `/sys` (read-only), and `/dev` (tmpfs with essential device nodes)
3. **`pivot_root`**: Replaces the root mount entirely (more secure than `chroot`, which only changes path resolution)

### 3. Resource Limits (`cgroups.cpp`)
Uses the cgroups v2 unified hierarchy:
- `memory.max` — hard memory limit (OOM kill on exceed)
- `memory.swap.max` — swap disabled to enforce strict limits
- `cpu.max` — CPU quota/period (e.g., 50ms/100ms = 50%)
- `pids.max` — process count cap (prevents fork bombs)

### 4. Networking (`network.cpp`)
Creates a veth pair connecting host and container:
- Host side: `172.20.0.1/24`
- Container side: `172.20.0.2/24` with default route via host

---

## Project Structure

```
mini-container/
├── CMakeLists.txt           # Build configuration
├── src/
│   ├── main.cpp             # CLI entry point
│   ├── container.cpp/.h     # Container lifecycle management
│   ├── namespaces.cpp/.h    # clone() + namespace isolation
│   ├── filesystem.cpp/.h    # OverlayFS + pivot_root + /proc /sys /dev
│   ├── cgroups.cpp/.h       # cgroups v2 resource limits
│   ├── network.cpp/.h       # veth pair networking
│   └── utils.cpp/.h         # Logging, syscall helpers, file I/O
├── tests/
│   └── test_container.sh    # Integration test suite
└── rootfs/                  # Alpine minirootfs (gitignored)
```

---

## License

MIT
