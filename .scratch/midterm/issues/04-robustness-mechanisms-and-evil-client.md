# 04: Four Toggleable Robustness Mechanisms and Adversarial Client (evil_client)

**What to build:** Four server-side robustness mechanisms enabled by default, paired with runtime flags (`--no-reap`, `--no-validate`, `--no-timeout`, `--no-limit`, and `--no-robust`) to disable them individually or all at once for live side-by-side comparison, plus an adversarial client executable (`evil_client`) designed to trigger each abnormal condition on demand.

**Blocked by:** 01: End-to-End Tracer Bullet (CMake, Shared Library Framing, Fork Server, and Client Echo/Quit)

**Status:** ready-for-agent

- [ ] **Zombie Reaping vs. `--no-reap`**: By default, the server reaps exited child workers immediately via `SIGCHLD` + `waitpid(WNOHANG)` leaving zero `<defunct>` processes; with `--no-reap`, exited workers remain visible as `<defunct>` zombies until server shutdown.
- [ ] **Packet Validation vs. `--no-validate`**: By default, the server validates protocol magic, version, message type, and maximum payload length (4 KiB cap), replying with an error frame and closing cleanly when `evil_client` sends bad magic or oversized length headers; with `--no-validate`, the worker attempts the unguarded path and terminates abnormally, demonstrating the failure mode without freezing the host machine.
- [ ] **Client Idle Timeout vs. `--no-timeout`**: By default, the server configures `SO_RCVTIMEO` (`-t <seconds>`, default 30s) so an idle `evil_client --idle` connection is timed out and cleaned up automatically; with `--no-timeout`, the child worker remains blocked waiting for data.
- [ ] **Connection Limit vs. `--no-limit`**: By default, the server tracks active child workers and rejects connections exceeding `-m <max>` (default 16) with a `RESP_BUSY` frame without forking a worker; with `--no-limit`, the server accepts connections beyond the limit.
- [ ] Automated CTest suite tests all four mechanisms in both guarded (default) and unguarded (`--no-*`) modes using `evil_client`.
