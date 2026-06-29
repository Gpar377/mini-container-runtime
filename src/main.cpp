#include "container.h"
#include "utils.h"

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <getopt.h>

// ─────────────────────────────────────────────────────────────────────────────
// CLI Usage
// ─────────────────────────────────────────────────────────────────────────────

static void print_usage(const char* progname) {
    std::cout << "\n"
        "  ┌─────────────────────────────────────────────────────────────┐\n"
        "  │         mini-container — Container Runtime from Scratch     │\n"
        "  └─────────────────────────────────────────────────────────────┘\n"
        "\n"
        "  Usage:\n"
        "    " << progname << " run [OPTIONS] <rootfs> <command> [args...]\n"
        "\n"
        "  Options:\n"
        "    --memory <MB>       Memory limit in megabytes (default: 256)\n"
        "    --cpus <fraction>   CPU limit as fraction (default: 1.0 = 100%)\n"
        "    --pids <max>        Max number of processes (default: 64)\n"
        "    --hostname <name>   Container hostname (default: \"container\")\n"
        "    --help              Show this help message\n"
        "\n"
        "  Examples:\n"
        "    # Run an interactive shell in an Alpine rootfs\n"
        "    sudo ./mini-container run ./rootfs /bin/sh\n"
        "\n"
        "    # Run with resource limits\n"
        "    sudo ./mini-container run --memory 128 --cpus 0.5 --pids 32 ./rootfs /bin/sh\n"
        "\n"
        "    # Run a specific command\n"
        "    sudo ./mini-container run ./rootfs /bin/echo \"Hello from container!\"\n"
        "\n"
        "  Notes:\n"
        "    - Must be run as root (sudo) for namespace/cgroup operations\n"
        "    - rootfs should be an extracted Linux root filesystem (e.g., Alpine minirootfs)\n"
        "\n";
}

// ─────────────────────────────────────────────────────────────────────────────
// Argument Parsing
// ─────────────────────────────────────────────────────────────────────────────

struct CliArgs {
    std::string rootfs;
    std::vector<std::string> command;
    CgroupLimits limits;
    std::string hostname = "container";
    bool valid = false;
};

static CliArgs parse_args(int argc, char* argv[]) {
    CliArgs args;

    if (argc < 2) {
        return args;
    }

    // First positional arg must be "run"
    std::string subcommand = argv[1];
    if (subcommand == "--help" || subcommand == "-h") {
        print_usage(argv[0]);
        exit(0);
    }
    if (subcommand != "run") {
        std::cerr << "Unknown subcommand: " << subcommand << "\n";
        std::cerr << "Use '" << argv[0] << " --help' for usage.\n";
        return args;
    }

    // Parse options after "run"
    int memory_mb = 256;
    double cpu_frac = 1.0;
    int pids = 64;

    int i = 2;
    while (i < argc) {
        std::string arg = argv[i];

        if (arg == "--memory" && i + 1 < argc) {
            memory_mb = std::atoi(argv[++i]);
        } else if (arg == "--cpus" && i + 1 < argc) {
            cpu_frac = std::atof(argv[++i]);
        } else if (arg == "--pids" && i + 1 < argc) {
            pids = std::atoi(argv[++i]);
        } else if (arg == "--hostname" && i + 1 < argc) {
            args.hostname = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            exit(0);
        } else if (arg[0] != '-') {
            // First non-option arg is rootfs, rest is the command
            args.rootfs = arg;
            for (int j = i + 1; j < argc; ++j) {
                args.command.push_back(argv[j]);
            }
            break;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return args;
        }
        ++i;
    }

    if (args.rootfs.empty() || args.command.empty()) {
        std::cerr << "Error: rootfs and command are required.\n";
        std::cerr << "Use '" << argv[0] << " --help' for usage.\n";
        return args;
    }

    // Apply parsed values to CgroupLimits
    args.limits.memory_max_bytes = static_cast<int64_t>(memory_mb) * 1024 * 1024;
    args.limits.cpu_quota_us = static_cast<int>(cpu_frac * 100000);
    args.limits.cpu_period_us = 100000;
    args.limits.pids_max = pids;
    args.valid = true;

    return args;
}

// ─────────────────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // Check for root privileges
    if (geteuid() != 0) {
        std::cerr << "\033[31mError: mini-container must be run as root (use sudo)\033[0m\n";
        return 1;
    }

    CliArgs args = parse_args(argc, argv);
    if (!args.valid) {
        print_usage(argv[0]);
        return 1;
    }

    std::cout << "\n"
        "  ┌─────────────────────────────────────────────────────────────┐\n"
        "  │                  mini-container starting                    │\n"
        "  └─────────────────────────────────────────────────────────────┘\n\n";

    try {
        int exit_code = run_container(
            args.rootfs,
            args.command,
            args.limits,
            args.hostname
        );

        std::cout << "\n"
            "  ┌─────────────────────────────────────────────────────────────┐\n"
            "  │              mini-container exited (code " << exit_code << ")                │\n"
            "  └─────────────────────────────────────────────────────────────┘\n\n";

        return exit_code;

    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Fatal: ") + e.what());
        return 1;
    }
}
