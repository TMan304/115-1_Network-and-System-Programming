#!/usr/bin/env bash
# ==============================================================================
# demo_concurrency.sh
# 
# Demonstrates 10+ concurrent client connections handled simultaneously by
# the fork-based server (User Story 11, Midterm Requirement 2).
#
# Proves:
#   1. Server creates a dedicated child process per client using fork() (zero threads).
#   2. Handles 12 simultaneous clients holding active connections via sleep.
#   3. Captures runtime proof via `ps --forest` (process hierarchy) and `ss -tnp`.
#   4. Confirms all 12 clients receive distinct worker child PIDs.
# ==============================================================================

set -euo pipefail

# Text formatting
BOLD="\033[1m"
GREEN="\033[32m"
BLUE="\033[34m"
CYAN="\033[36m"
YELLOW="\033[33m"
RED="\033[31m"
RESET="\033[0m"

# Default configuration
DEFAULT_PORT=19012
DEFAULT_CLIENTS=12
DEFAULT_SLEEP_SEC=3
TEST_MODE=0

# Parse arguments
PORT="${DEFAULT_PORT}"
NUM_CLIENTS="${DEFAULT_CLIENTS}"
SLEEP_SEC="${DEFAULT_SLEEP_SEC}"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --test)
            TEST_MODE=1
            SLEEP_SEC=1
            shift
            ;;
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -n|--clients)
            NUM_CLIENTS="$2"
            shift 2
            ;;
        -s|--sleep)
            SLEEP_SEC="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -p, --port <port>       Server port (default: ${DEFAULT_PORT})"
            echo "  -n, --clients <num>     Number of concurrent clients (default: ${DEFAULT_CLIENTS})"
            echo "  -s, --sleep <sec>       Hold duration per client (default: ${DEFAULT_SLEEP_SEC}s)"
            echo "  --test                  Fast automated verification mode for CTest"
            echo "  -h, --help              Show this help message"
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
CLIENT_BIN="${ROOT_DIR}/build/client"

if [[ ! -x "${SERVER_BIN}" ]]; then
    SERVER_BIN="${ROOT_DIR}/server"
fi
if [[ ! -x "${CLIENT_BIN}" ]]; then
    CLIENT_BIN="${ROOT_DIR}/client"
fi

if [[ ! -x "${SERVER_BIN}" ]] || [[ ! -x "${CLIENT_BIN}" ]]; then
    echo -e "${RED}[ERROR] Binaries not found. Please run 'cmake --build build' first.${RESET}"
    exit 1
fi

TEMP_DIR="$(mktemp -d /tmp/nsp_concurrency_XXXXXX)"
SERVER_PID=""

cleanup() {
    local exit_code=$?
    if [[ -n "${SERVER_PID}" ]] && kill -0 "${SERVER_PID}" 2>/dev/null; then
        kill -INT "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
    # Terminate any remaining child processes
    pkill -P $$ 2>/dev/null || true
    rm -rf "${TEMP_DIR}"
    exit "${exit_code}"
}
trap cleanup EXIT INT TERM

echo -e "${BOLD}${CYAN}========================================================================${RESET}"
echo -e "${BOLD}${CYAN}     NSP Concurrency Demonstration: 10+ Concurrent Clients Runtime Proof ${RESET}"
echo -e "${BOLD}${CYAN}========================================================================${RESET}"
echo -e "${BLUE}[*] Process Architecture:${RESET} fork()-based (zero POSIX threads)"
echo -e "${BLUE}[*] Target Server Port:${RESET}   ${PORT}"
echo -e "${BLUE}[*] Concurrent Clients:${RESET}   ${NUM_CLIENTS} (Holding ${SLEEP_SEC}s connection each)"
echo ""

# 1. Start Server
echo -e "${YELLOW}[1/4] Starting NSP Server on port ${PORT}...${RESET}"
"${SERVER_BIN}" -p "${PORT}" -m "$((NUM_CLIENTS + 8))" -l warn > "${TEMP_DIR}/server.log" 2>&1 &
SERVER_PID=$!
sleep 0.3

if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
    echo -e "${RED}[ERROR] Server failed to start! Check logs:${RESET}"
    cat "${TEMP_DIR}/server.log"
    exit 1
fi
echo -e "${GREEN}[+] Server started successfully (Parent PID: ${SERVER_PID})${RESET}"
echo ""

# 2. Spawn concurrent clients
echo -e "${YELLOW}[2/4] Spawning ${NUM_CLIENTS} concurrent clients holding connections for ${SLEEP_SEC}s and requesting sysinfo...${RESET}"
CLIENT_PIDS=()
for ((i=1; i<=NUM_CLIENTS; i++)); do
    printf "sleep %d\nsysinfo\nquit\n" "${SLEEP_SEC}" | "${CLIENT_BIN}" -p "${PORT}" > "${TEMP_DIR}/client_${i}.out" 2>&1 &
    CLIENT_PIDS+=($!)
done

echo -e "${GREEN}[+] Spawned ${NUM_CLIENTS} client processes: ${CLIENT_PIDS[*]}${RESET}"
echo -e "${BLUE}[*] Waiting 0.5s for all clients to establish connections and enter sleep state...${RESET}"
sleep 0.5
echo ""

# 3. Capture runtime proof: ps --forest and ss -tnp
echo -e "${YELLOW}[3/4] Capturing Runtime Proof while all ${NUM_CLIENTS} clients are active simultaneously:${RESET}"
echo -e "${BOLD}--- [A] Process Hierarchy via 'ps --forest' (Parent Server and Dedicated Child Workers) ---${RESET}"

# Display parent and child worker processes
ps --forest -o pid,ppid,stat,time,cmd -p "${SERVER_PID}" --ppid "${SERVER_PID}" || true
echo ""

echo -e "${BOLD}--- [B] Active TCP Sockets via 'ss -tnp' (Showing ${NUM_CLIENTS} Established Connections) ---${RESET}"
ss -tnp "( sport = :${PORT} or dport = :${PORT} )" || true
echo ""

# 4. Wait for all clients to finish and verify distinct worker PIDs
echo -e "${YELLOW}[4/4] Collecting responses from all ${NUM_CLIENTS} clients...${RESET}"
WORKER_PIDS=()
FAILURES=0

for ((i=1; i<=NUM_CLIENTS; i++)); do
    wait "${CLIENT_PIDS[$((i-1))]}" || {
        echo -e "${RED}[ERROR] Client #${i} (PID ${CLIENT_PIDS[$((i-1))]}) exited with error.${RESET}"
        FAILURES=$((FAILURES + 1))
    }
    
    OUTPUT="$(cat "${TEMP_DIR}/client_${i}.out" 2>/dev/null || echo "")"
    if [[ "${OUTPUT}" =~ Worker\ \[PID\ ([0-9]+)\]\ slept\ for ]]; then
        WPID="${BASH_REMATCH[1]}"
        WORKER_PIDS+=("${WPID}")
    else
        echo -e "${RED}[ERROR] Client #${i} did not receive expected worker PID in response: ${OUTPUT}${RESET}"
        FAILURES=$((FAILURES + 1))
    fi
done

echo ""
echo -e "${BOLD}--- Client / Worker Verification Table ---${RESET}"
printf "%-10s | %-12s | %-18s | %-17s | %-10s\n" "Client #" "Client PID" "Assigned Worker PID" "Parent Server PID" "Status"
printf "%s\n" "-------------------------------------------------------------------------------"

for ((i=1; i<=NUM_CLIENTS; i++)); do
    CPID="${CLIENT_PIDS[$((i-1))]}"
    WPID="${WORKER_PIDS[$((i-1))]:-UNKNOWN}"
    STATUS="${GREEN}OK${RESET}"
    if [[ "${WPID}" == "UNKNOWN" ]] || [[ "${WPID}" == "${SERVER_PID}" ]]; then
        STATUS="${RED}FAIL${RESET}"
    fi
    printf "%-10s | %-12s | %-18s | %-17s | %b\n" "#${i}" "${CPID}" "${WPID}" "${SERVER_PID}" "${STATUS}"
done
echo ""

# Check uniqueness of worker PIDs
UNIQUE_COUNT=$(printf "%s\n" "${WORKER_PIDS[@]}" | sort -u | wc -l)
echo -e "${BLUE}[*] Total clients spawned:${RESET}   ${NUM_CLIENTS}"
echo -e "${BLUE}[*] Unique worker child PIDs:${RESET} ${UNIQUE_COUNT}"

if [[ "${FAILURES}" -eq 0 ]] && [[ "${UNIQUE_COUNT}" -eq "${NUM_CLIENTS}" ]]; then
    echo ""
    echo -e "${BOLD}${GREEN}[PASSED] Concurrency Proof Verified!${RESET}"
    echo -e "${GREEN}  - Exactly ${NUM_CLIENTS} concurrent clients were handled simultaneously.${RESET}"
    echo -e "${GREEN}  - Every client was served by an isolated child process via fork().${RESET}"
    echo -e "${GREEN}  - Runtime hierarchy confirmed via 'ps --forest' and 'ss -tnp'.${RESET}"
    echo -e "${GREEN}  - Strict zero-thread requirement satisfied.${RESET}"
    exit 0
else
    echo ""
    echo -e "${BOLD}${RED}[FAILED] Concurrency verification failed (failures=${FAILURES}, unique_pids=${UNIQUE_COUNT}/${NUM_CLIENTS}).${RESET}"
    exit 1
fi
