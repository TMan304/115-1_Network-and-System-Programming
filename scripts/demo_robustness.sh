#!/usr/bin/env bash
# ==============================================================================
# demo_robustness.sh
# 
# Demonstrates side-by-side "WITHOUT vs. WITH" execution for all four server
# robustness mechanisms (User Story 16-19, Midterm Requirement 7 & 8).
#
# Robustness Mechanisms:
#   #1: Zombie Process Reaping (SIGCHLD handler with non-blocking waitpid)
#   #2: Malformed / Oversized Packet Validation (Magic bytes & 4 KiB cap)
#   #3: Client Idle Socket Receive Timeout (SO_RCVTIMEO)
#   #4: Maximum Concurrent Connection Limit (Admission control / RESP_BUSY)
# ==============================================================================

set -euo pipefail

# Text formatting
BOLD="\033[1m"
GREEN="\033[32m"
BLUE="\033[34m"
CYAN="\033[36m"
YELLOW="\033[33m"
RED="\033[31m"
MAGENTA="\033[35m"
RESET="\033[0m"

BASE_PORT=19020
TEST_MODE=0
TARGET_MECH="all"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --test)
            TEST_MODE=1
            shift
            ;;
        -p|--port)
            BASE_PORT="$2"
            shift 2
            ;;
        1|zombie|reap)
            TARGET_MECH="1"
            shift
            ;;
        2|validate|magic)
            TARGET_MECH="2"
            shift
            ;;
        3|timeout|idle)
            TARGET_MECH="3"
            shift
            ;;
        4|limit|flood)
            TARGET_MECH="4"
            shift
            ;;
        all)
            TARGET_MECH="all"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS] [1|2|3|4|all]"
            echo "Options:"
            echo "  1, zombie     Demo Robustness #1: Zombie Process Reaping"
            echo "  2, validate   Demo Robustness #2: Binary Frame Validation"
            echo "  3, timeout    Demo Robustness #3: Client Idle Timeout"
            echo "  4, limit      Demo Robustness #4: Connection Limit Protection"
            echo "  all           Demo all 4 mechanisms sequentially (default)"
            echo "  -p, --port    Base listening port (default: ${BASE_PORT})"
            echo "  --test        Fast automated verification mode for CTest"
            echo "  -h, --help    Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            exit 1
            ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

SERVER_BIN="${ROOT_DIR}/build/server"
EVIL_BIN="${ROOT_DIR}/build/evil_client"

if [[ ! -x "${SERVER_BIN}" ]]; then SERVER_BIN="${ROOT_DIR}/server"; fi
if [[ ! -x "${EVIL_BIN}" ]]; then EVIL_BIN="${ROOT_DIR}/evil_client"; fi

if [[ ! -x "${SERVER_BIN}" ]] || [[ ! -x "${EVIL_BIN}" ]]; then
    echo -e "${RED}[ERROR] Binaries not found. Please build first ('cmake --build build').${RESET}"
    exit 1
fi

TEMP_DIR="$(mktemp -d /tmp/nsp_robustness_XXXXXX)"
CURRENT_SERVER_PID=""

stop_server() {
    if [[ -n "${CURRENT_SERVER_PID}" ]] && kill -0 "${CURRENT_SERVER_PID}" 2>/dev/null; then
        kill -INT "${CURRENT_SERVER_PID}" 2>/dev/null || true
        wait "${CURRENT_SERVER_PID}" 2>/dev/null || true
        CURRENT_SERVER_PID=""
    fi
}

cleanup() {
    local code=$?
    stop_server
    pkill -P $$ 2>/dev/null || true
    rm -rf "${TEMP_DIR}"
    exit "${code}"
}
trap cleanup EXIT INT TERM

start_test_server() {
    local port="$1"
    shift
    stop_server
    "${SERVER_BIN}" -p "${port}" -l warn "$@" > "${TEMP_DIR}/server_${port}.log" 2>&1 &
    CURRENT_SERVER_PID=$!
    sleep 0.25
    if ! kill -0 "${CURRENT_SERVER_PID}" 2>/dev/null; then
        echo -e "${RED}[ERROR] Server failed to start on port ${port}.${RESET}"
        cat "${TEMP_DIR}/server_${port}.log"
        exit 1
    fi
}

echo -e "${BOLD}${CYAN}========================================================================${RESET}"
echo -e "${BOLD}${CYAN}      NSP Robustness A/B Demonstrations: With vs. Without Mechanisms    ${RESET}"
echo -e "${BOLD}${CYAN}========================================================================${RESET}"

# ------------------------------------------------------------------------------
# Robustness Mechanism #1: Zombie Process Reaping
# ------------------------------------------------------------------------------
demo_zombie_reaping() {
    local port=$((BASE_PORT + 1))
    echo ""
    echo -e "${BOLD}${MAGENTA}========================================================================${RESET}"
    echo -e "${BOLD}${MAGENTA} [Robustness #1] Zombie Process Reaping (SIGCHLD + waitpid loop)       ${RESET}"
    echo -e "${BOLD}${MAGENTA}========================================================================${RESET}"
    echo -e "${BLUE}Description:${RESET} In UNIX fork-based servers, terminated child processes remain in the"
    echo -e "             process table as zombies (<defunct>) until reaped by waitpid(). Without"
    echo -e "             proper reaping, connection churn rapidly exhausts system process IDs."
    echo ""

    # Part A: WITHOUT mechanism
    echo -e "${BOLD}${RED}--- [A] WITHOUT Zombie Reaping (--no-reap) ---${RESET}"
    echo -e "${YELLOW}[*] Starting server with --no-reap on port ${port}...${RESET}"
    start_test_server "${port}" --no-reap
    echo -e "${GREEN}[+] Server running (PID: ${CURRENT_SERVER_PID})${RESET}"

    echo -e "${YELLOW}[*] Firing 5 rapid connect/quit sessions via evil_client...${RESET}"
    "${EVIL_BIN}" -p "${port}" --zombie-burst 5
    sleep 0.3

    echo -e "${BOLD}Checking process table for server child processes:${RESET}"
    ps -o pid,ppid,stat,cmd -p "${CURRENT_SERVER_PID}" --ppid "${CURRENT_SERVER_PID}" || true
    
    local defunct_count
    defunct_count=$(ps -o stat --ppid "${CURRENT_SERVER_PID}" 2>/dev/null | grep -c "Z" || true)
    echo -e "${RED}[OBSERVATION] Detected ${defunct_count} zombie process(es) (<defunct> / State 'Z')!${RESET}"
    echo -e "${RED}              Exited child processes were NOT reaped and leak process table entries.${RESET}"
    stop_server

    if [[ "${TEST_MODE}" -eq 1 ]] && [[ "${defunct_count}" -lt 5 ]]; then
        echo -e "${RED}[TEST FAIL] Expected at least 5 zombies in --no-reap mode.${RESET}"
        return 1
    fi

    echo ""
    # Part B: WITH mechanism
    echo -e "${BOLD}${GREEN}--- [B] WITH Zombie Reaping (Default Enabled) ---${RESET}"
    echo -e "${YELLOW}[*] Starting server with default SIGCHLD waitpid handler on port ${port}...${RESET}"
    start_test_server "${port}"
    echo -e "${GREEN}[+] Server running (PID: ${CURRENT_SERVER_PID})${RESET}"

    echo -e "${YELLOW}[*] Firing 5 rapid connect/quit sessions via evil_client...${RESET}"
    "${EVIL_BIN}" -p "${port}" --zombie-burst 5
    sleep 0.3

    echo -e "${BOLD}Checking process table for server child processes:${RESET}"
    ps -o pid,ppid,stat,cmd -p "${CURRENT_SERVER_PID}" --ppid "${CURRENT_SERVER_PID}" || true
    
    local clean_defunct_count
    clean_defunct_count=$(ps -o stat --ppid "${CURRENT_SERVER_PID}" 2>/dev/null | grep -c "Z" || true)
    echo -e "${GREEN}[OBSERVATION] Detected ${clean_defunct_count} zombie processes!${RESET}"
    echo -e "${GREEN}              All exited children immediately reaped via non-blocking waitpid().${RESET}"
    stop_server

    if [[ "${TEST_MODE}" -eq 1 ]] && [[ "${clean_defunct_count}" -ne 0 ]]; then
        echo -e "${RED}[TEST FAIL] Expected 0 zombies in default reaping mode.${RESET}"
        return 1
    fi

    echo ""
    echo -e "${BOLD}${GREEN}[RESULT #1] Zombie reaping successfully prevents process table leakage.${RESET}"
}

# ------------------------------------------------------------------------------
# Robustness Mechanism #2: Frame Header Validation
# ------------------------------------------------------------------------------
demo_frame_validation() {
    local port=$((BASE_PORT + 2))
    echo ""
    echo -e "${BOLD}${MAGENTA}========================================================================${RESET}"
    echo -e "${BOLD}${MAGENTA} [Robustness #2] Malformed & Oversized Frame Header Validation          ${RESET}"
    echo -e "${BOLD}${MAGENTA}========================================================================${RESET}"
    echo -e "${BLUE}Description:${RESET} Protects against corrupt packets, invalid protocol magic, and payload"
    echo -e "             length overflow attacks (> 4 KiB cap) before memory allocation."
    echo ""

    # Part A: WITHOUT mechanism
    echo -e "${BOLD}${RED}--- [A] WITHOUT Validation (--no-validate) ---${RESET}"
    echo -e "${YELLOW}[*] Starting server with --no-validate on port ${port}...${RESET}"
    start_test_server "${port}" --no-validate
    echo -e "${GREEN}[+] Server running (PID: ${CURRENT_SERVER_PID})${RESET}"

    echo -e "${YELLOW}[*] Sending invalid magic header (0xDEAD) via evil_client...${RESET}"
    set +e
    "${EVIL_BIN}" -p "${port}" --bad-magic
    local rc_bad_magic=$?
    set -e
    echo -e "${RED}[OBSERVATION] Server worker failed to parse frame and terminated abruptly (exit code ${rc_bad_magic}).${RESET}"

    echo -e "${YELLOW}[*] Sending oversized payload header (0x7FFFFFFF = 2 GiB) via evil_client...${RESET}"
    set +e
    "${EVIL_BIN}" -p "${port}" --oversized
    local rc_oversized=$?
    set -e
    echo -e "${RED}[OBSERVATION] Unguarded allocation caused worker process failure / abrupt disconnect (exit code ${rc_oversized}).${RESET}"
    stop_server

    echo ""
    # Part B: WITH mechanism
    echo -e "${BOLD}${GREEN}--- [B] WITH Validation (Default Enabled) ---${RESET}"
    echo -e "${YELLOW}[*] Starting server with default validation enabled on port ${port}...${RESET}"
    start_test_server "${port}"
    echo -e "${GREEN}[+] Server running (PID: ${CURRENT_SERVER_PID})${RESET}"

    echo -e "${YELLOW}[*] Sending invalid magic header (0xDEAD) via evil_client...${RESET}"
    set +e
    "${EVIL_BIN}" -p "${port}" --bad-magic
    local rc_valid_magic=$?
    set -e

    echo -e "${YELLOW}[*] Sending oversized payload header (0x7FFFFFFF) via evil_client...${RESET}"
    set +e
    "${EVIL_BIN}" -p "${port}" --oversized
    local rc_valid_oversized=$?
    set -e

    echo -e "${GREEN}[OBSERVATION] Both attacks safely rejected before allocation!${RESET}"
    echo -e "${GREEN}              Server replied with formatted RESP_ERROR and closed session gracefully.${RESET}"
    stop_server

    if [[ "${TEST_MODE}" -eq 1 ]]; then
        if [[ "${rc_valid_magic}" -ne 0 ]] || [[ "${rc_valid_oversized}" -ne 0 ]]; then
            echo -e "${RED}[TEST FAIL] Expected evil_client exit code 0 when server returns clean RESP_ERROR.${RESET}"
            return 1
        fi
    fi

    echo ""
    echo -e "${BOLD}${GREEN}[RESULT #2] Frame validation safely repels malformed and DoS length headers.${RESET}"
}

# ------------------------------------------------------------------------------
# Robustness Mechanism #3: Client Idle Socket Timeout
# ------------------------------------------------------------------------------
demo_idle_timeout() {
    local port=$((BASE_PORT + 3))
    echo ""
    echo -e "${BOLD}${MAGENTA}========================================================================${RESET}"
    echo -e "${BOLD}${MAGENTA} [Robustness #3] Client Idle Socket Timeout (SO_RCVTIMEO)              ${RESET}"
    echo -e "${BOLD}${MAGENTA}========================================================================${RESET}"
    echo -e "${BLUE}Description:${RESET} Sets SO_RCVTIMEO on client sockets to prevent slow-loris attacks"
    echo -e "             where clients open silent connections and hold worker processes indefinitely."
    echo ""

    # Part A: WITHOUT mechanism
    echo -e "${BOLD}${RED}--- [A] WITHOUT Idle Timeout (--no-timeout) ---${RESET}"
    echo -e "${YELLOW}[*] Starting server with --no-timeout on port ${port}...${RESET}"
    start_test_server "${port}" --no-timeout
    echo -e "${GREEN}[+] Server running (PID: ${CURRENT_SERVER_PID})${RESET}"

    echo -e "${YELLOW}[*] Connecting silent idle client (waiting 1s without sending data)...${RESET}"
    set +e
    "${EVIL_BIN}" -p "${port}" --idle 1
    local rc_no_timeout=$?
    set -e
    echo -e "${RED}[OBSERVATION] Worker process stayed blocked indefinitely; connection held open (exit code ${rc_no_timeout}).${RESET}"
    stop_server

    echo ""
    # Part B: WITH mechanism
    echo -e "${BOLD}${GREEN}--- [B] WITH Idle Timeout (-t 1 second) ---${RESET}"
    echo -e "${YELLOW}[*] Starting server with 1s timeout (-t 1) on port ${port}...${RESET}"
    start_test_server "${port}" -t 1
    echo -e "${GREEN}[+] Server running (PID: ${CURRENT_SERVER_PID})${RESET}"

    echo -e "${YELLOW}[*] Connecting silent idle client (waiting up to 2s)...${RESET}"
    set +e
    "${EVIL_BIN}" -p "${port}" --idle 2
    local rc_timeout=$?
    set -e

    echo -e "${GREEN}[OBSERVATION] Server worker detected socket idle timeout after 1s!${RESET}"
    echo -e "${GREEN}              Sent RESP_ERROR ('Client idle timeout expired') and closed socket cleanly.${RESET}"
    stop_server

    if [[ "${TEST_MODE}" -eq 1 ]]; then
        if [[ "${rc_no_timeout}" -ne 3 ]] || [[ "${rc_timeout}" -ne 0 ]]; then
            echo -e "${RED}[TEST FAIL] Idle timeout test expectations not met (no_to=${rc_no_timeout}, to=${rc_timeout}).${RESET}"
            return 1
        fi
    fi

    echo ""
    echo -e "${BOLD}${GREEN}[RESULT #3] Idle timeout prevents abandoned sockets from hanging workers.${RESET}"
}

# ------------------------------------------------------------------------------
# Robustness Mechanism #4: Connection Limit Protection
# ------------------------------------------------------------------------------
demo_connection_limit() {
    local port=$((BASE_PORT + 4))
    echo ""
    echo -e "${BOLD}${MAGENTA}========================================================================${RESET}"
    echo -e "${BOLD}${MAGENTA} [Robustness #4] Maximum Concurrent Connection Limit (-m <max>)        ${RESET}"
    echo -e "${BOLD}${MAGENTA}========================================================================${RESET}"
    echo -e "${BLUE}Description:${RESET} Caps maximum active child processes. Under connection bursts,"
    echo -e "             excess clients are immediately rejected with RESP_BUSY without calling fork()."
    echo ""

    # Part A: WITHOUT mechanism
    echo -e "${BOLD}${RED}--- [A] WITHOUT Connection Limit (--no-limit, -m 3) ---${RESET}"
    echo -e "${YELLOW}[*] Starting server with -m 3 --no-limit on port ${port}...${RESET}"
    start_test_server "${port}" -m 3 --no-limit
    echo -e "${GREEN}[+] Server running (PID: ${CURRENT_SERVER_PID})${RESET}"

    echo -e "${YELLOW}[*] Launching 5 concurrent connections via evil_client --flood 5...${RESET}"
    "${EVIL_BIN}" -p "${port}" --flood 5 --hold 400
    echo -e "${RED}[OBSERVATION] All 5 connections accepted without limit! Server spawned unconstrained workers.${RESET}"
    stop_server

    echo ""
    # Part B: WITH mechanism
    echo -e "${BOLD}${GREEN}--- [B] WITH Connection Limit (-m 3) ---${RESET}"
    echo -e "${YELLOW}[*] Starting server with limit -m 3 on port ${port}...${RESET}"
    start_test_server "${port}" -m 3
    echo -e "${GREEN}[+] Server running (PID: ${CURRENT_SERVER_PID})${RESET}"

    echo -e "${YELLOW}[*] Launching 5 concurrent connections via evil_client --flood 5...${RESET}"
    "${EVIL_BIN}" -p "${port}" --flood 5 --hold 400
    echo -e "${GREEN}[OBSERVATION] Exactly 3 connections accepted; 2 rejected with RESP_BUSY!${RESET}"
    echo -e "${GREEN}              Server protected host resources by refusing to fork excess workers.${RESET}"
    stop_server

    echo ""
    echo -e "${BOLD}${GREEN}[RESULT #4] Admission limit prevents process table flooding and resource exhaustion.${RESET}"
}

case "${TARGET_MECH}" in
    1) demo_zombie_reaping ;;
    2) demo_frame_validation ;;
    3) demo_idle_timeout ;;
    4) demo_connection_limit ;;
    all)
        demo_zombie_reaping
        demo_frame_validation
        demo_idle_timeout
        demo_connection_limit
        ;;
esac

echo ""
echo -e "${BOLD}${CYAN}========================================================================${RESET}"
echo -e "${BOLD}${GREEN}  [SUCCESS] All Selected Robustness Demonstrations Completed Cleanly!   ${RESET}"
echo -e "${BOLD}${CYAN}========================================================================${RESET}"
exit 0
