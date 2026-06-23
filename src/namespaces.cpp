#include "namespaces.h"
#include "utils.h"

#include <sched.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <stdexcept>

// Stack size for the cloned child process (1 MB)
static constexpr size_t STACK_SIZE = 1024 * 1024;

// ─────────────────────────────────────────────────────────────────────────────
// Trampoline: bridge between C-style clone() callback and C++ std::function
// ─────────────────────────────────────────────────────────────────────────────

struct CloneArgs {
    std::function<int()>* func;
};

static int clone_trampoline(void* arg) {
    auto* args = static_cast<CloneArgs*>(arg);
    return (*args->func)();
}

// ─────────────────────────────────────────────────────────────────────────────
// Create Isolated Process
// ─────────────────────────────────────────────────────────────────────────────

pid_t create_isolated_process(std::function<int()> child_func) {
    // Allocate stack for the child process
    // clone() requires us to provide a stack because it doesn't use fork()'s COW.
    // The stack grows downward on x86-64, so we pass the TOP of the allocation.
    char* stack = new char[STACK_SIZE];
    char* stack_top = stack + STACK_SIZE;

    // Namespace flags:
    //   CLONE_NEWPID  — new PID namespace (container is PID 1)
    //   CLONE_NEWNS   — new mount namespace (isolated mounts)
    //   CLONE_NEWUTS  — new UTS namespace (isolated hostname)
    //   CLONE_NEWIPC  — new IPC namespace (isolated semaphores, shared memory)
    //   CLONE_NEWNET  — new network namespace (isolated network stack)
    //   SIGCHLD       — send SIGCHLD to parent when child exits
    int clone_flags = CLONE_NEWPID
                    | CLONE_NEWNS
                    | CLONE_NEWUTS
                    | CLONE_NEWIPC
                    | CLONE_NEWNET
                    | SIGCHLD;

    CloneArgs args;
    args.func = &child_func;

    LOG_INFO("Creating isolated process with namespaces: PID|MNT|UTS|IPC|NET");

    pid_t child_pid = clone(
        clone_trampoline,
        stack_top,
        clone_flags,
        &args
    );

    if (child_pid == -1) {
        delete[] stack;
        throw std::runtime_error(
            std::string("clone() failed: ") + std::strerror(errno)
        );
    }

    LOG_INFO("Child process created with PID " + std::to_string(child_pid));

    // NOTE: We intentionally leak the stack here.
    // In a production runtime, we'd free it after waitpid().
    // For this project, the parent process lifetime is short enough
    // that OS cleanup handles it. A proper fix would use RAII or
    // store the pointer for cleanup after wait.

    return child_pid;
}

// ─────────────────────────────────────────────────────────────────────────────
// Set Container Hostname
// ─────────────────────────────────────────────────────────────────────────────

void set_container_hostname(const std::string& hostname) {
    syscall_check(
        sethostname(hostname.c_str(), hostname.size()),
        "sethostname(\"" + hostname + "\")"
    );
    LOG_INFO("Hostname set to: " + hostname);
}
