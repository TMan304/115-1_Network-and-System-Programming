# 04: Four Toggleable Robustness Mechanisms and Adversarial Client (evil_client)

**What to build:** Four server-side robustness mechanisms enabled by default, paired with runtime flags (`--no-reap`, `--no-validate`, `--no-timeout`, `--no-limit`, and `--no-robust`) to disable them individually or all at once for live side-by-side comparison, plus an adversarial client executable (`evil_client`) designed to trigger each abnormal condition on demand.

**Blocked by:** 01: End-to-End Tracer Bullet (CMake, Shared Library Framing, Fork Server, and Client Echo/Quit)

**Status:** ready-for-human

- [x] **Zombie Reaping vs. `--no-reap`**: By default, the server reaps exited child workers immediately via `SIGCHLD` + `waitpid(WNOHANG)` leaving zero `<defunct>` processes; with `--no-reap`, exited workers remain visible as `<defunct>` zombies until server shutdown.
- [x] **Packet Validation vs. `--no-validate`**: By default, the server validates protocol magic, version, message type, and maximum payload length (4 KiB cap), replying with an error frame and closing cleanly when `evil_client` sends bad magic or oversized length headers; with `--no-validate`, the worker attempts the unguarded path and terminates abnormally, demonstrating the failure mode without freezing the host machine.
- [x] **Client Idle Timeout vs. `--no-timeout`**: By default, the server configures `SO_RCVTIMEO` (`-t <seconds>`, default 30s) so an idle `evil_client --idle` connection is timed out and cleaned up automatically; with `--no-timeout`, the child worker remains blocked waiting for data.
- [x] **Connection Limit vs. `--no-limit`**: By default, the server tracks active child workers and rejects connections exceeding `-m <max>` (default 16) with a `RESP_BUSY` frame without forking a worker; with `--no-limit`, the server accepts connections beyond the limit.
- [x] Automated CTest suite tests all four mechanisms in both guarded (default) and unguarded (`--no-*`) modes using `evil_client`.

## Comments

### Implementation Summary
1. **Adversarial Client (`src/evil_client/main.c`)**:
   - Implemented dedicated testing executable dynamically linked to `libnsp.so`.
   - Supports:
     - `--bad-magic`: Sends invalid magic (`0xDEAD`), tests clean error rejection (`RESP_ERROR`, exit 0) vs. unguarded worker crash/disconnect (exit 2).
     - `--oversized`: Sends 8-byte header with `payload_len = 0x7FFFFFFF`, tests clean rejection (`RESP_ERROR`, exit 0) vs. unguarded worker crash/disconnect (exit 2).
     - `--idle [sec]`: Opens silent connection with receive timeout, tests server timeout rejection (`RESP_ERROR`, exit 0) vs. unclosed connection persisting (exit 3).
     - `--flood <count>`: Opens concurrent connections with polling for immediate `RESP_BUSY` or echo response, reporting `Flood result: accepted=<A> busy=<B>`.
     - `--zombie-burst <count>`: Issues sequential quit requests to trigger rapid child worker exits for inspecting `/proc` zombie state.
     - `--host`, `--port`, `--hold <ms>`, and `--help`.

2. **Server Robustness Mechanisms (`src/server/server.c`, `src/server/main.c`, `src/lib/io.c`)**:
   - **Mechanism 1 (Zombie Reaping)**: Guarded mode reaps exited workers with `SIGCHLD` + `waitpid(WNOHANG)` and logs abnormal exits; `--no-reap` mode disables waitpid and disables active client count tracking so zombies accumulate as `<defunct>` in `/proc` without hitting connection limits.
   - **Mechanism 2 (Packet Validation)**: In `src/lib/io.c`, unguarded `--no-validate` mode detects invalid headers (`magic != 0x4E53`, `version != 1`, `len > 4096`), logs fatal error, closes socket, and terminates worker abnormally with `_exit(1)` instead of sending `RESP_ERROR`. Guarded mode validates headers cleanly and sends `RESP_ERROR`.
   - **Mechanism 3 (Idle Timeout)**: Guarded mode enforces `SO_RCVTIMEO` (`-t <sec>`), sending `RESP_ERROR` on timeout; `--no-timeout` mode skips `SO_RCVTIMEO`.
   - **Mechanism 4 (Connection Limit)**: Rechecks zombies before checking limit; rejects connections beyond `-m <max>` with `RESP_BUSY` when limit is enabled; bypassed with `--no-limit`.
   - CLI in `src/server/main.c` updated with `-v` / `--verbose` (sets `NSP_LOG_DEBUG`, coordinating with Issue 03) and all `--no-*` flags.

3. **Automated Robustness Tests (`tests/test_robustness.c`)**:
   - Added 8 automated test cases covering both guarded and unguarded modes across all 4 mechanisms on dedicated ports `19321`–`19328`.
   - All tests pass 100% under strict flags (`-Wall -Wextra -Werror -pedantic`).
