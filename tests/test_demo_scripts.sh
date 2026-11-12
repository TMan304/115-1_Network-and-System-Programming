#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${ROOT_DIR}"

echo "=== Testing Demo Scripts ==="

# Check scripts exist and are executable
test_scripts=(
    "scripts/demo_concurrency.sh"
    "scripts/demo_packet_capture.sh"
    "scripts/demo_robustness.sh"
    "scripts/demo_logging.sh"
    "scripts/run_all_demos.sh"
)

for s in "${test_scripts[@]}"; do
    if [ ! -f "$s" ]; then
        echo "FAIL: Expected script $s does not exist"
        exit 1
    fi
    if [ ! -x "$s" ]; then
        echo "FAIL: Expected script $s is not executable"
        exit 1
    fi
done

echo "[1/4] Running Concurrency Demo in test mode..."
./scripts/demo_concurrency.sh --test

echo "[2/4] Running Robustness Demo in test mode..."
./scripts/demo_robustness.sh --test

echo "[3/4] Running Two-Tier Logging Demo in test mode..."
./scripts/demo_logging.sh --test

echo "[4/4] Running Packet Capture Demo in test mode..."
./scripts/demo_packet_capture.sh --test

echo "=== All Demo Scripts Passed in Test Mode! ==="
exit 0
