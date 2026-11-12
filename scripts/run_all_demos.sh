#!/usr/bin/env bash
# ==============================================================================
# run_all_demos.sh
# 
# Master Demonstration Runner for Network and System Programming Midterm.
# Runs all required demonstrations with interactive pauses (ideal for live
# classroom presentation and 2-minute demo video recording) or in batch mode.
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

PAUSE_BETWEEN=1
TEST_MODE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --test|--batch|--no-pause)
            PAUSE_BETWEEN=0
            if [[ "$1" == "--test" ]]; then
                TEST_MODE=1
            fi
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo "Options:"
            echo "  --no-pause, --batch   Run all demos sequentially without pausing"
            echo "  --test                Run all demos in fast test verification mode"
            echo "  -h, --help            Show this help message"
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

cd "${ROOT_DIR}"

pause_prompt() {
    local step_title="$1"
    if [[ "${PAUSE_BETWEEN}" -eq 1 ]] && [[ -t 0 ]]; then
        echo ""
        echo -e "${YELLOW}>>> Press [ENTER] to start: ${BOLD}${step_title}${RESET} (or Ctrl+C to cancel)..."
        read -r
    fi
}

echo -e "${BOLD}${CYAN}========================================================================${RESET}"
echo -e "${BOLD}${CYAN}  Network and System Programming Midterm - Live Demonstration Suite     ${RESET}"
echo -e "${BOLD}${CYAN}========================================================================${RESET}"
echo -e "This suite demonstrates all required midterm features:"
echo -e "  1. Dynamic Shared Library (.so) Linking Verification ('ldd')"
echo -e "  2. 10+ Concurrent Clients Runtime Proof ('ps --forest' & 'ss -tnp')"
echo -e "  3. Packet Capture & Binary Frame Analysis ('tcpdump -nn -X -r')"
echo -e "  4. Four Robustness Mechanisms (Side-by-side 'WITH vs. WITHOUT')"
echo -e "  5. Two-Tier Debug Logging (Compile-Time Gate & Runtime Level Filter)"
echo ""

# ------------------------------------------------------------------------------
# 1. Dynamic Shared Library Verification
# ------------------------------------------------------------------------------
pause_prompt "[Demo 1/5] Dynamic Library Linking Verification (libnsp.so)"
echo -e "${BOLD}${MAGENTA}========================================================================${RESET}"
echo -e "${BOLD}${MAGENTA} [Demo 1/5] Dynamic Library Linking Verification via 'ldd'               ${RESET}"
echo -e "${BOLD}${MAGENTA}========================================================================${RESET}"
echo -e "${BLUE}Requirement:${RESET} Common client/server functions encapsulated in shared dynamic library (libnsp.so)."
echo ""

for bin in build/server build/client build/evil_client; do
    if [[ -x "${bin}" ]]; then
        echo -e "${BOLD}Checking ${bin}:${RESET}"
        ldd "${bin}" | grep -E "libnsp.so|statically linked" || true
        echo ""
    fi
done

# ------------------------------------------------------------------------------
# 2. Concurrency Runtime Proof
# ------------------------------------------------------------------------------
pause_prompt "[Demo 2/5] 10+ Concurrent Clients Runtime Proof"
if [[ "${TEST_MODE}" -eq 1 ]]; then
    ./scripts/demo_concurrency.sh --test
else
    ./scripts/demo_concurrency.sh -n 12 -s 3
fi

# ------------------------------------------------------------------------------
# 3. Packet Capture and Wire Protocol Analysis
# ------------------------------------------------------------------------------
pause_prompt "[Demo 3/5] Packet Capture & Wire Protocol Analysis (tcpdump)"
if [[ "${TEST_MODE}" -eq 1 ]]; then
    ./scripts/demo_packet_capture.sh --test
else
    ./scripts/demo_packet_capture.sh
fi

# ------------------------------------------------------------------------------
# 4. Four Robustness Mechanisms A/B Demonstrations
# ------------------------------------------------------------------------------
pause_prompt "[Demo 4/5] Four Robustness Mechanisms A/B Comparisons"
if [[ "${TEST_MODE}" -eq 1 ]]; then
    ./scripts/demo_robustness.sh --test
else
    ./scripts/demo_robustness.sh all
fi

# ------------------------------------------------------------------------------
# 5. Two-Tier Debug Logging Control
# ------------------------------------------------------------------------------
pause_prompt "[Demo 5/5] Two-Tier Debug Logging Control"
if [[ "${TEST_MODE}" -eq 1 ]]; then
    ./scripts/demo_logging.sh --test
else
    ./scripts/demo_logging.sh
fi

echo ""
echo -e "${BOLD}${CYAN}========================================================================${RESET}"
echo -e "${BOLD}${GREEN}  [ALL PASSED] All Demonstration Modules Completed Successfully!       ${RESET}"
echo -e "${BOLD}${CYAN}========================================================================${RESET}"
exit 0
