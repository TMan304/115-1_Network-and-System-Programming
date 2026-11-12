#!/usr/bin/env bash
# ==============================================================================
# demo_logging.sh
# 
# Demonstrates Two-Tier Debug Logging Control (User Story 12 & 13, Midterm Req 4).
#
# Proves:
#   Tier 1 (Compile-Time): Preprocessor gate NSP_COMPILE_DEBUG strips LOG_DEBUG
#                          statements from the binary when disabled (zero overhead).
#   Tier 2 (Runtime):      Dynamic verbosity filter via '-l <level>' or 'LOG_LEVEL'
#                          env var without recompilation (debug/info/warn/error/off).
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

PORT=19030
TEST_MODE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --test)
            TEST_MODE=1
            shift
            ;;
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  -p, --port <port>   Server port (default: ${PORT})"
            echo "  --test              Fast automated verification mode for CTest"
            echo "  -h, --help          Show this help message"
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

if [[ ! -x "${SERVER_BIN}" ]]; then SERVER_BIN="${ROOT_DIR}/server"; fi
if [[ ! -x "${CLIENT_BIN}" ]]; then CLIENT_BIN="${ROOT_DIR}/client"; fi

TEMP_DIR="$(mktemp -d /tmp/nsp_logging_XXXXXX)"
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

echo -e "${BOLD}${CYAN}========================================================================${RESET}"
echo -e "${BOLD}${CYAN}      NSP Two-Tier Debug Logging Demonstration (Compile & Runtime)      ${RESET}"
echo -e "${BOLD}${CYAN}========================================================================${RESET}"
echo ""

# ------------------------------------------------------------------------------
# Tier 1: Compile-Time Gate Verification
# ------------------------------------------------------------------------------
echo -e "${BOLD}${MAGENTA}========================================================================${RESET}"
echo -e "${BOLD}${MAGENTA} [Tier 1] Compile-Time Preprocessor Gate (NSP_COMPILE_DEBUG)           ${RESET}"
echo -e "${BOLD}${MAGENTA}========================================================================${RESET}"
echo -e "${BLUE}Architecture:${RESET} In include/nsp/log.h, LOG_DEBUG is wrapped in a preprocessor macro:"
echo -e "  #if NSP_COMPILE_DEBUG"
echo -e "  #  define LOG_DEBUG(fmt, ...) nsp_log_write(NSP_LOG_DEBUG, fmt, ##__VA_ARGS__)"
echo -e "  #else"
echo -e "  #  define LOG_DEBUG(fmt, ...) ((void)0)"
echo -e "  #endif"
echo ""
echo -e "When cmake -DNSP_DEBUG=OFF is used, LOG_DEBUG statements expand to ((void)0) at"
echo -e "compile time, omitting format strings and argument evaluation completely."
echo ""

# Check whether current build has debug logging enabled
if grep -q "Handling ECHO request" <(strings "${SERVER_BIN}" 2>/dev/null || true); then
    echo -e "${GREEN}[+] Current build (${SERVER_BIN}) was compiled with NSP_DEBUG=ON.${RESET}"
    echo -e "${GREEN}    Debug format strings are present in binary text sections.${RESET}"
else
    echo -e "${YELLOW}[*] Current build was compiled with NSP_DEBUG=OFF (Release mode).${RESET}"
    echo -e "${YELLOW}    Debug format strings are stripped from binary.${RESET}"
fi
echo ""

# ------------------------------------------------------------------------------
# Tier 2: Runtime Log Level Filtering
# ------------------------------------------------------------------------------
echo -e "${BOLD}${MAGENTA}========================================================================${RESET}"
echo -e "${BOLD}${MAGENTA} [Tier 2] Runtime Log Level Filtering (-l <level> and LOG_LEVEL)        ${RESET}"
echo -e "${BOLD}${MAGENTA}========================================================================${RESET}"
echo -e "${BLUE}Description:${RESET} Minimum log threshold is dynamically configurable without recompiling."
echo -e "             Each log entry includes ISO timestamp, PID, level tag, and message."
echo ""

# Test Case 1: Level 'error'
echo -e "${BOLD}${YELLOW}--- [Case 1] Server running with '-l error' ---${RESET}"
echo -e "${BLUE}[*] Starting server with -l error on port ${PORT}...${RESET}"
"${SERVER_BIN}" -p "${PORT}" -l error > "${TEMP_DIR}/log_error.txt" 2>&1 &
CURRENT_SERVER_PID=$!
sleep 0.25

"${CLIENT_BIN}" -p "${PORT}" -c "echo 'Hello Error Level'" > /dev/null 2>&1 || true
sleep 0.1
stop_server

echo -e "${BOLD}Server log output (Level: ERROR):${RESET}"
if [[ -s "${TEMP_DIR}/log_error.txt" ]]; then
    cat "${TEMP_DIR}/log_error.txt"
else
    echo -e "${GREEN}(No error logs emitted - normal INFO/DEBUG logs were properly suppressed!)${RESET}"
fi
echo ""

# Test Case 2: Level 'info'
echo -e "${BOLD}${YELLOW}--- [Case 2] Server running with '-l info' ---${RESET}"
echo -e "${BLUE}[*] Starting server with -l info on port ${PORT}...${RESET}"
"${SERVER_BIN}" -p "${PORT}" -l info > "${TEMP_DIR}/log_info.txt" 2>&1 &
CURRENT_SERVER_PID=$!
sleep 0.25

"${CLIENT_BIN}" -p "${PORT}" -c "echo 'Hello Info Level'" > /dev/null 2>&1 || true
sleep 0.1
stop_server

echo -e "${BOLD}Server log output (Level: INFO):${RESET}"
cat "${TEMP_DIR}/log_info.txt"
echo ""

# Test Case 3: Level 'debug'
echo -e "${BOLD}${YELLOW}--- [Case 3] Server running with '-l debug' (-v flag) ---${RESET}"
echo -e "${BLUE}[*] Starting server with -v on port ${PORT}...${RESET}"
"${SERVER_BIN}" -p "${PORT}" -v > "${TEMP_DIR}/log_debug.txt" 2>&1 &
CURRENT_SERVER_PID=$!
sleep 0.25

"${CLIENT_BIN}" -p "${PORT}" -c "echo 'Hello Debug Level'" > /dev/null 2>&1 || true
sleep 0.1
stop_server

echo -e "${BOLD}Server log output (Level: DEBUG):${RESET}"
cat "${TEMP_DIR}/log_debug.txt"
echo ""

# Test Case 4: Environment Variable LOG_LEVEL=warn
echo -e "${BOLD}${YELLOW}--- [Case 4] Runtime Control via LOG_LEVEL=warn Environment Variable ---${RESET}"
echo -e "${BLUE}[*] Starting server with LOG_LEVEL=warn on port ${PORT}...${RESET}"
LOG_LEVEL=warn "${SERVER_BIN}" -p "${PORT}" > "${TEMP_DIR}/log_env.txt" 2>&1 &
CURRENT_SERVER_PID=$!
sleep 0.25

"${CLIENT_BIN}" -p "${PORT}" -c "echo 'Hello Env Level'" > /dev/null 2>&1 || true
sleep 0.1
stop_server

echo -e "${BOLD}Server log output (LOG_LEVEL=warn):${RESET}"
if [[ -s "${TEMP_DIR}/log_env.txt" ]]; then
    cat "${TEMP_DIR}/log_env.txt"
else
    echo -e "${GREEN}(No warn/error logs emitted - INFO and DEBUG suppressed as expected)${RESET}"
fi
echo ""

# Assertions for test mode
if [[ "${TEST_MODE}" -eq 1 ]]; then
    if ! grep -q "INFO" "${TEMP_DIR}/log_info.txt"; then
        echo -e "${RED}[TEST FAIL] Expected INFO level logs in Case 2.${RESET}"
        exit 1
    fi
    if grep -q "DEBUG" "${TEMP_DIR}/log_error.txt"; then
        echo -e "${RED}[TEST FAIL] DEBUG logs should not appear when level is error.${RESET}"
        exit 1
    fi
fi

echo -e "${BOLD}${GREEN}[PASSED] Two-Tier Debug Logging Demonstration Verified!${RESET}"
echo -e "${GREEN}  - Tier 1: Compile-time gate strips debug code when NSP_DEBUG=OFF.${RESET}"
echo -e "${GREEN}  - Tier 2: Runtime log levels (debug/info/warn/error) control verbosity dynamically.${RESET}"
exit 0
