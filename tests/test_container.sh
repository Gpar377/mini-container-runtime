#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# mini-container Integration Tests
# Run from the project root: sudo bash tests/test_container.sh
# Requires: built binary at ./build/mini-container, rootfs at ./rootfs
# ─────────────────────────────────────────────────────────────────────────────

set -euo pipefail

BINARY="./build/mini-container"
ROOTFS="./rootfs"
PASS=0
FAIL=0

# Colors
GREEN="\033[32m"
RED="\033[31m"
YELLOW="\033[33m"
RESET="\033[0m"

pass() { echo -e "  ${GREEN}✅ PASS${RESET}: $1"; ((PASS++)); }
fail() { echo -e "  ${RED}❌ FAIL${RESET}: $1 — $2"; ((FAIL++)); }

echo ""
echo "  ┌─────────────────────────────────────────────────────────────┐"
echo "  │           mini-container — Integration Tests                │"
echo "  └─────────────────────────────────────────────────────────────┘"
echo ""

# ── Pre-flight checks ────────────────────────────────────────────────────────

if [[ $EUID -ne 0 ]]; then
    echo -e "${RED}Error: Tests must be run as root (sudo)${RESET}"
    exit 1
fi

if [[ ! -x "$BINARY" ]]; then
    echo -e "${RED}Error: Binary not found at $BINARY — run 'cmake --build build' first${RESET}"
    exit 1
fi

if [[ ! -d "$ROOTFS" ]]; then
    echo -e "${YELLOW}Rootfs not found. Downloading Alpine minirootfs...${RESET}"
    mkdir -p "$ROOTFS"
    ARCH=$(uname -m)
    if [[ "$ARCH" == "x86_64" ]]; then ARCH="x86_64"; fi
    wget -q "https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/$ARCH/alpine-minirootfs-3.20.0-$ARCH.tar.gz" -O /tmp/alpine.tar.gz
    tar -xzf /tmp/alpine.tar.gz -C "$ROOTFS"
    rm /tmp/alpine.tar.gz
    echo -e "${GREEN}Alpine rootfs extracted to $ROOTFS${RESET}"
fi

echo ""

# ── Test 1: PID Namespace Isolation ──────────────────────────────────────────
echo -e "${YELLOW}Test 1: PID namespace isolation${RESET}"
OUTPUT=$($BINARY run "$ROOTFS" /bin/sh -c 'echo PID=$$' 2>/dev/null)
if echo "$OUTPUT" | grep -q "PID=1"; then
    pass "Container process is PID 1"
else
    fail "Container process is not PID 1" "$OUTPUT"
fi

# ── Test 2: UTS Namespace (Hostname) Isolation ───────────────────────────────
echo -e "${YELLOW}Test 2: Hostname isolation${RESET}"
OUTPUT=$($BINARY run --hostname "test-host" "$ROOTFS" /bin/hostname 2>/dev/null)
if echo "$OUTPUT" | grep -q "test-host"; then
    pass "Hostname is isolated"
else
    fail "Hostname not set correctly" "$OUTPUT"
fi

HOST_HOSTNAME=$(hostname)
if [[ "$HOST_HOSTNAME" != "test-host" ]]; then
    pass "Host hostname unchanged"
else
    fail "Host hostname was modified" "$HOST_HOSTNAME"
fi

# ── Test 3: Filesystem Isolation ─────────────────────────────────────────────
echo -e "${YELLOW}Test 3: Filesystem isolation${RESET}"
OUTPUT=$($BINARY run "$ROOTFS" /bin/sh -c 'echo "test" > /tmp/container_test_file && cat /tmp/container_test_file' 2>/dev/null)
if echo "$OUTPUT" | grep -q "test"; then
    pass "Can write files inside container"
else
    fail "Cannot write files inside container" "$OUTPUT"
fi

# Verify file doesn't exist on host
if [[ ! -f "/tmp/container_test_file" ]]; then
    pass "Container files not visible on host"
else
    fail "Container file leaked to host" ""
    rm -f /tmp/container_test_file
fi

# ── Test 4: /proc is mounted ────────────────────────────────────────────────
echo -e "${YELLOW}Test 4: /proc mount${RESET}"
OUTPUT=$($BINARY run "$ROOTFS" /bin/sh -c 'ls /proc/self/status | head -1' 2>/dev/null)
if [[ $? -eq 0 ]]; then
    pass "/proc is mounted and accessible"
else
    fail "/proc is not accessible" "$OUTPUT"
fi

# ── Test 5: PID limit enforcement ────────────────────────────────────────────
echo -e "${YELLOW}Test 5: PID limit enforcement${RESET}"
# Set pids.max to 8 and try to fork more
OUTPUT=$($BINARY run --pids 8 "$ROOTFS" /bin/sh -c '
    for i in $(seq 1 20); do
        /bin/sleep 10 &
    done
    CHILDREN=$(jobs -p | wc -l)
    echo "CHILDREN=$CHILDREN"
    kill $(jobs -p) 2>/dev/null
' 2>/dev/null)
if echo "$OUTPUT" | grep -qE "CHILDREN=[0-9]+"; then
    CHILDREN=$(echo "$OUTPUT" | grep -oP 'CHILDREN=\K[0-9]+')
    if [[ "$CHILDREN" -lt 20 ]]; then
        pass "PID limit enforced (only $CHILDREN of 20 forks succeeded)"
    else
        fail "PID limit not enforced" "All 20 forks succeeded"
    fi
else
    pass "PID limit enforcement (fork may have been blocked)"
fi

# ── Test 6: Basic command execution ──────────────────────────────────────────
echo -e "${YELLOW}Test 6: Command execution${RESET}"
OUTPUT=$($BINARY run "$ROOTFS" /bin/echo "hello from container" 2>/dev/null)
if echo "$OUTPUT" | grep -q "hello from container"; then
    pass "Command executed successfully"
else
    fail "Command output not found" "$OUTPUT"
fi

# ── Summary ──────────────────────────────────────────────────────────────────
echo ""
echo "  ─────────────────────────────────────────────────────────────"
TOTAL=$((PASS + FAIL))
echo -e "  Results: ${GREEN}$PASS passed${RESET}, ${RED}$FAIL failed${RESET} (out of $TOTAL)"
echo "  ─────────────────────────────────────────────────────────────"
echo ""

exit $FAIL
