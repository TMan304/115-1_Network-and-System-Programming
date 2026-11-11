# 02: System Monitoring and Concurrency Commands (sysinfo, time, procs, sleep)

**What to build:** Full remote system monitoring and concurrency inspection capabilities between client and server. Users can issue `sysinfo` to retrieve the server's hostname, OS/kernel release, uptime, 1/5/15-minute load averages, CPU model and core count, and total/available memory; `time` to query the server's current epoch and formatted local/UTC clock; `procs` to inspect the dedicated worker child PID, parent server PID, active concurrent client count, and total system process count; and `sleep <seconds>` to hold a worker connection open for concurrency testing.

**Blocked by:** 01: End-to-End Tracer Bullet (CMake, Shared Library Framing, Fork Server, and Client Echo/Quit)

**Status:** ready-for-human

- [x] `sysinfo` command returns a structured report containing non-empty hostname, kernel/OS version, uptime, load averages, CPU model/core count, and memory statistics gathered from standard Linux interfaces.
- [x] `time` command returns the server's current timestamp and formatted time string.
- [x] `procs` command returns the serving child's PID (`getpid()`), parent server PID (`getppid()`), active concurrent client count, and system process count.
- [x] `sleep <seconds>` pauses the serving child worker for the requested duration (within safe bounds) and replies with the worker PID and elapsed time.
- [x] Automated end-to-end test launches at least 10 concurrent clients holding overlapping connections and verifies that all 10 succeed simultaneously and are served by distinct child worker PIDs.

## Comments

### Implementation Summary
1. **Server Command Handlers (`src/server/handlers.h`, `src/server/handlers.c`)**:
   - `handle_cmd_sysinfo`: Gathers system metrics via `gethostname()`, `uname()`, `sysinfo()`, `/proc/loadavg`, `/proc/cpuinfo`, and `/proc/meminfo`. Formats and sends back a multi-line report detailing Hostname, OS/Kernel, Uptime (seconds and formatted d/h/m/s), Load Averages (1, 5, 15 min), CPU Model, CPU Core count, and Memory (Total, Free, Available in kB and MiB) with `NSP_MSG_RESP_OK`.
   - `handle_cmd_time`: Obtains `time(NULL)`, formats local time via `localtime_r` + `strftime` with timezone, and formats UTC time via `gmtime_r` + `strftime`. Sends back epoch and timestamps with `NSP_MSG_RESP_OK`.
   - `handle_cmd_procs`: Reports worker child PID (`getpid()`), parent server PID (`ctx->parent_pid` / `getppid()`), active concurrent client count (`ctx->active_clients`), and system process count (`sysinfo(&si)` -> `si.procs`) with `NSP_MSG_RESP_OK`.
   - `handle_cmd_sleep`: Validates integer sleep duration in range `1 <= sec <= 60` from payload. Returns `NSP_MSG_RESP_ERROR` on missing/invalid input. Sleeps using `sleep((unsigned int)sec)` and returns `NSP_MSG_RESP_OK` reporting child worker PID and elapsed seconds.

2. **Client CLI Support (`src/client/client.c`)**:
   - Registered `sysinfo`, `time`, `procs`, and `sleep` in `g_cli_commands`.
   - Updated `help` command output to display all 7 commands (`sysinfo`, `echo`, `time`, `procs`, `sleep`, `help`, `quit`).
   - Ensured both interactive REPL mode and non-interactive single-command mode (`-c "<cmd>"`) handle all monitoring commands seamlessly.
   - Cleaned up single-command exit sequence to receive server `RESP_BYE` acknowledgement frame without polluting stdout.

3. **Automated End-to-End Test Suite (`tests/test_sysinfo_concurrency.c`)**:
   - Implemented automated E2E tests executing against server on test port `19120`:
     - `test_cmd_sysinfo`: Asserts non-empty values for Hostname, OS/Kernel, Uptime, Load Average, CPU Model & Cores, Memory Total/Free/Available.
     - `test_cmd_time`: Asserts Epoch, Local formatted time, and UTC formatted time.
     - `test_cmd_procs`: Asserts worker PID > 0, parent PID matching server PID, active clients >= 1, and system process count > 0.
     - `test_cmd_sleep`: Asserts successful sleep response and verifies error handling on invalid/out-of-range sleep durations (0, 999, negative, string, missing arg).
     - `test_client_help_all_commands`: Asserts all 7 commands are present in `help`.
     - `test_client_interactive_repl_all_commands`: Piped interactive REPL execution of all commands in a single session.
     - `test_concurrency_12_clients`: Spawns 12 concurrent client processes holding overlapping connections executing `sleep 1`. All 12 completed in 1.00s total wall time, and all 12 were confirmed to be served by 12 distinct worker child PIDs.
