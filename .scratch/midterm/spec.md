# Midterm: Fork-Based Remote System Monitor Client-Server Application

Status: ready-for-agent

## Problem Statement

For the Network and System Programming midterm project, we need a multi-client C client-server application that demonstrates core UNIX system and network programming concepts in a reproducible, verifiable manner during live classroom demonstrations and packet capture inspections. Specifically, naive socket programs fail under real-world conditions: they leave zombie processes (`<defunct>`) when child workers exit, crash or hang on partial reads/writes or malformed packets, get stuck indefinitely when clients go idle, exhaust system process limits under connection floods, and lack structured compile-time and runtime diagnostic controls. Furthermore, duplicated protocol and I/O logic across client and server binaries leads to maintenance drift unless properly encapsulated in a shared dynamic library.

## Solution

Build a C-based Remote System Monitor client-server system built with CMake and linked against a shared dynamic library (`libnsp.so`). The server uses `fork()` to spawn a dedicated child process per client connection (strictly no threads), handles 10+ concurrent clients with runtime proof, and serves system monitoring and diagnostic commands over a custom framed binary TCP protocol (`magic + type + length + payload`).

Both client and server share packet framing, reliable stream I/O (`send_all` / `recv_all` with `EINTR` retry and partial transfer handling), and a two-tier logging subsystem (compile-time preprocessor gate + runtime level filter) via `libnsp.so` configured with RPATH so binaries run without manual environment setup.

To satisfy and demonstrate the robustness requirement, the server implements four toggleable robustness mechanisms—Zombie Process Reaping (`SIGCHLD` + `waitpid`), Packet Validation (magic & payload cap), Client Idle Timeout (`SO_RCVTIMEO`), and Maximum Concurrent Connection Limit—alongside baseline socket hygiene (`SO_REUSEADDR`, `SIGPIPE` suppression, graceful `SIGINT`/`SIGTERM` shutdown). A companion adversarial client (`evil_client`) and automated helper scripts allow side-by-side live comparisons of server behavior with and without each robustness mechanism enabled, as well as `tcpdump` loopback packet capture and 10+ concurrent client load verification.

## User Stories

1. As a user running the client interactively, I want to connect to the server over TCP using a configurable host and port (defaulting to `127.0.0.1:9000`), so that I can issue commands and view responses in a command-line REPL.
2. As a user running the client in single-command mode, I want to pass a command directly via command-line arguments and exit upon receiving the response, so that scripts and automated tests can invoke client requests non-interactively.
3. As a user querying system status, I want to run the `sysinfo` command to retrieve the server's hostname, OS/kernel version, system uptime, 1/5/15-minute load averages, CPU model and core count, and total/available memory, so that I can inspect the remote host's health in one request.
4. As a user testing connectivity, I want to run `echo <text>` and receive the exact text back from the server, so that I can verify round-trip framing and payload integrity.
5. As a user checking server clock state, I want to run `time` to get the server's current timestamp and formatted local/UTC time, so that I can verify server responsiveness and time synchronization.
6. As a user inspecting server concurrency, I want to run `procs` to see the worker child process PID serving my connection, the parent server PID, the server's active concurrent client count, and the total system process count, so that I can confirm each client is served by an isolated `fork()` child process.
7. As a user running concurrency demonstrations, I want to run `sleep <seconds>` to instruct my dedicated server child process to pause before replying, so that multiple concurrent connections remain active simultaneously for inspection via `ps` and `ss`.
8. As a user exploring the client CLI, I want to run `help` to list all supported commands and their syntax, so that I know how to interact with the application without reading source code.
9. As a user finishing a session, I want to run `quit` (or send EOF / Ctrl-D) to cleanly close the session with a server farewell acknowledgement, so that both client and server worker processes terminate gracefully.
10. As a system administrator starting the server, I want every incoming client connection to be handled by a newly `fork()`ed child process without using POSIX threads, so that process isolation is guaranteed and the strict `fork()` requirement is met.
11. As a demonstrator proving concurrency, I want to run a benchmark/concurrency mode (or script) that launches at least 10 simultaneous clients holding active connections while the server logs active connection counts and child PIDs, so that I have clear runtime proof of handling 10+ concurrent clients.
12. As a developer building for production vs. debugging, I want a compile-time CMake option (`NSP_DEBUG=ON/OFF`) that completely strips `LOG_DEBUG` statements at the preprocessor level when disabled, so that debug logging incurs zero runtime overhead in non-debug builds.
13. As a user running a debug-enabled build, I want to control the minimum log verbosity at runtime via a CLI flag (`-l debug|info|warn|error` or `-v`) or the `LOG_LEVEL` environment variable, so that I can adjust diagnostic output without recompiling.
14. As a developer maintaining the project, I want protocol framing, reliable socket read/write helpers, and logging utilities compiled into a dynamic shared library (`libnsp.so`) dynamically linked by the server, standard client, and adversarial client with embedded RPATH, so that shared code resides in a single `.so` artifact and executables run directly without setting `LD_LIBRARY_PATH`.
15. As a network analyst, I want a clear binary packet header (`magic`, `type`, `length` in network byte order followed by payload) and a capture script using `tcpdump` on the loopback interface, so that I can capture `.pcap` traces and clearly identify protocol fields in `tcpdump -X` or Wireshark.
16. As a demonstrator showcasing Zombie Process Reaping (Robustness #1), I want the server by default to install a `SIGCHLD` handler using `waitpid(-1, ..., WNOHANG)` in a loop to reap exited children immediately, and a `--no-reap` flag to disable it, so that I can demonstrate `<defunct>` zombie accumulation in `ps` when disabled versus clean process tables when enabled.
17. As a demonstrator showcasing Malformed/Oversized Packet Protection (Robustness #2), I want the server by default to validate the protocol magic bytes, message type, and maximum payload length (4 KiB cap) before allocating memory or reading payload bytes, and a `--no-validate` flag to bypass checks, so that I can use `evil_client` to show how garbage headers or huge length fields cause allocation failure/misbehavior when unguarded versus clean error rejection and disconnection when guarded.
18. As a demonstrator showcasing Client Idle Timeout (Robustness #3), I want the server child process by default to enforce a configurable socket receive timeout (`-t <sec>`, default 30s) via `SO_RCVTIMEO`, and a `--no-timeout` flag to disable it, so that I can use `evil_client` to open a silent connection and show the worker process hanging forever when unguarded versus timing out and cleaning up automatically when guarded.
19. As a demonstrator showcasing Connection Limit Protection (Robustness #4), I want the server parent process by default to track active child workers and reject new connections (sending an error/busy frame and closing the socket) when the configurable maximum (`-m <max>`, default 16) is reached, plus a `--no-limit` flag to disable the cap, so that I can show resource/process exhaustion prevention under connection floods.
20. As a server operator restarting the server frequently during demos, I want the server listening socket to set `SO_REUSEADDR` and ignore `SIGPIPE`, so that restarting the server never fails with `Address already in use` and abrupt client disconnects during writes do not kill server processes via unhandled signals.
21. As a server operator stopping the server with `Ctrl+C`, I want the server parent process to catch `SIGINT`/`SIGTERM`, close the listening socket, terminate and reap any remaining child workers, and log a clean shutdown summary, so that no orphan worker processes remain listening or running.
22. As a student preparing the project build and live demo, I want the project built via CMake with a concise guide and self-contained shell scripts for the 10-client concurrency proof, `tcpdump` capture, and robustness A/B comparisons, so that I can execute every required demonstration reliably on my own machine.

## Implementation Decisions

### 1. System Architecture & Process Model
- **Three Executables + One Shared Library**:
  - **Shared Library (`libnsp.so`)**: Encapsulates protocol framing/validation, reliable stream I/O, and the two-tier logging subsystem. Dynamically linked by all executables.
  - **Server (`server`)**: Single-threaded parent listener that accepts TCP connections, tracks active child PIDs, enforces the connection limit, forks a child process per accepted client, and reaps exited children via `SIGCHLD`. Child processes close the listening descriptor, apply socket timeouts, and run the request-response loop for their assigned client.
  - **Standard Client (`client`)**: Supports both an interactive line-oriented REPL and non-interactive single-command or batch/concurrency execution modes (`-c <cmd>`), linking against `libnsp.so`.
  - **Adversarial Client (`evil_client`)**: A dedicated testing/demo tool linking against `libnsp.so` (and raw sockets) capable of injecting bad magic headers, oversized payload lengths (`0x7FFFFFFF`), truncated frames, abrupt half-open disconnects, idle silent connections, and rapid connection bursts.
- **No Threads**: POSIX threads (`pthread`) are strictly prohibited across the entire codebase; all concurrency uses `fork()` and process-level signaling.

### 2. Wire Protocol Design (Binary Framed TCP)
- **Header Format**: Fixed 8-byte header in network byte order (big-endian):
  - `magic` (`uint16_t`, 2 bytes): Fixed constant `0x4E53` (`"NS"` in ASCII, easy to spot in `tcpdump -X` hex dumps).
  - `version` (`uint8_t`, 1 byte): Protocol version `0x01`.
  - `msg_type` (`uint8_t`, 1 byte): Message type enum:
    - Requests: `REQ_SYSINFO (0x01)`, `REQ_ECHO (0x02)`, `REQ_TIME (0x03)`, `REQ_PROCS (0x04)`, `REQ_SLEEP (0x05)`, `REQ_QUIT (0x06)`
    - Responses: `RESP_OK (0x80)`, `RESP_ERROR (0x81)`, `RESP_BUSY (0x82)`, `RESP_BYE (0x83)`
  - `payload_len` (`uint32_t`, 4 bytes): Payload byte length in network byte order (`0` to `4096` bytes max under normal validation).
- **Payload**: UTF-8 text or structured key-value text of `payload_len` bytes immediately following the 8-byte header (not null-terminated on the wire; null-terminated in memory by the receiver).

### 3. Shared Library (`libnsp.so`) Interface
- **Reliable Stream I/O Module**:
  - `nsp_send_all(fd, buf, len)`: Loops over `send(..., MSG_NOSIGNAL)` until all `len` bytes are written, retrying transparently on `EINTR` and returning error codes on `EPIPE`/`ECONNRESET` or timeout.
  - `nsp_recv_all(fd, buf, len)`: Loops over `recv(...)` until exactly `len` bytes are read, EOF (`0` returned) is encountered, or an error/timeout (`EAGAIN`/`EWOULDBLOCK`) occurs, retrying on `EINTR`.
- **Protocol Framing Module**:
  - `nsp_send_frame(fd, msg_type, payload, payload_len)`: Packs the 8-byte header in network byte order and sends header + payload via `nsp_send_all`.
  - `nsp_recv_frame(fd, *hdr_out, **payload_out, validate_enabled)`: Reads the 8-byte header via `nsp_recv_all`, converts fields from network to host byte order, and if `validate_enabled` is true, verifies `magic == 0x4E53`, `version == 1`, valid `msg_type`, and `payload_len <= NSP_MAX_PAYLOAD (4096)`. When `validate_enabled` is false (`--no-validate` mode), skips validation and attempts to `malloc(payload_len)` and read directly (capped or caught safely enough to demonstrate the failure mode deterministically without freezing the host OS).
- **Two-Tier Logging Module**:
  - **Compile-Time Control**: Controlled by preprocessor macro `NSP_COMPILE_DEBUG`. When `NSP_COMPILE_DEBUG` is `0` (default release build), `LOG_DEBUG(fmt, ...)` expands to `((void)0)` so arguments are not evaluated and format strings are omitted from the binary. When `NSP_COMPILE_DEBUG` is `1` (`cmake -DNSP_DEBUG=ON`), `LOG_DEBUG` calls into the runtime logger.
  - **Runtime Control**: `LOG_INFO`, `LOG_WARN`, `LOG_ERROR` (and `LOG_DEBUG` when compiled in) check the runtime threshold configured via `nsp_log_set_level(level)` or initialized from the `LOG_LEVEL` environment variable / `-l <level>` CLI flag. Each log line outputs timestamp, PID, level tag, and message.

### 4. Server Features & System Information Gathering
- **`sysinfo`**: Reads kernel and system metrics using standard Linux interfaces (`uname()`, `sysinfo()`, `/proc/cpuinfo`, `/proc/meminfo`, `/proc/loadavg`, `gethostname()`) and formats a multi-line human-readable report.
- **`echo <text>`**: Returns the client's payload prefixed or verbatim with `RESP_OK`.
- **`time`**: Formats server epoch time, local ISO-8601 time, and timezone.
- **`procs`**: Reports the handling child's `getpid()`, parent's `getppid()`, active client count passed down at fork time, and total system processes from `sysinfo()`.
- **`sleep <sec>`**: Parses `<sec>` (bounded to a safe range, e.g., 1–60s), sleeps, and returns a completion message with child PID and elapsed time.
- **`help`**: Handled locally by the client (or echoed by server) listing available commands.
- **`quit`**: Sends `REQ_QUIT`, receives `RESP_BYE`, and closes the connection cleanly.

### 5. Robustness Mechanisms & Toggle Flags
- **Default Configuration**:
  - Port: `9000` (`-p <port>`)
  - Max concurrent clients: `16` (`-m <max>`)
  - Client idle receive timeout: `30` seconds (`-t <seconds>`)
  - Max payload size: `4096` bytes
- **Runtime Toggle Flags for A/B Demonstration**:
  - `--no-reap`: Disables installing the `SIGCHLD` `waitpid` handler (and avoids `SA_NOCLDWAIT`), causing exited child workers to remain as `<defunct>` zombies in the process table, and prevents active connection counter decrement on exit unless reaped.Wait—to keep `--no-reap` focused purely on zombie creation without accidentally blocking new connections via the connection limit, when `--no-reap` is active the server either disables the connection counter decrement or pairs with `--no-limit`. Better yet: in `--no-reap` mode, the server deliberately skips `waitpid()` so children stay `<defunct>` in `ps`, while only counting limit when limit is enabled.
  - `--no-validate`: Passes `validate_enabled = false` to `nsp_recv_frame()`. When `evil_client --bad-magic` or `evil_client --oversized` sends a crafted header, the child worker attempts the unchecked operation, logs a fatal unguarded error / aborts the child worker (demonstrating worker crash/failure), whereas with validation enabled it logs a warning, replies with `RESP_ERROR`, and closes the connection cleanly.
  - `--no-timeout`: Skips setting `SO_RCVTIMEO` on the accepted client socket. When `evil_client --idle` connects and sends nothing, the child worker stays blocked in `recv()` indefinitely (visible in `ps`), whereas with timeout enabled the child logs an idle timeout after `-t` seconds, sends an error/timeout frame, and exits cleanly.
  - `--no-limit`: Disables checking active child count against `-m <max>`. With limit enabled (e.g., `server -m 3`), the 4th concurrent client immediately receives `RESP_BUSY ("Server busy: max connections reached")` and is disconnected without forking a worker.
  - `--no-robust`: Convenience shorthand that enables `--no-reap`, `--no-validate`, `--no-timeout`, and `--no-limit` simultaneously.

### 6. Build System & Demonstration Scripts
- **CMake Build**:
  - Top-level `CMakeLists.txt` targeting C11 (`-Wall -Wextra -Werror`), building shared library target `nsp` (`libnsp.so`) and executables `server`, `client`, `evil_client`, plus test executables registered with `enable_testing()` / CTest.
  - Sets `CMAKE_BUILD_RPATH` so binaries in `build/` locate `libnsp.so` automatically.
  - Option `option(NSP_DEBUG "Enable compile-time debug logging" ON)` (or configurable via `-DNSP_DEBUG=ON/OFF`).
- **Demonstration Scripts**:
  - Concurrency proof script: Starts server, launches 12 concurrent clients executing `sleep 3` + `sysinfo`, runs `ps --forest` and `ss -tnp` while all 12 are active, and prints the summary.
  - Packet capture script: Runs a sample session while capturing loopback traffic via `tcpdump -i lo -w capture.pcap port <port>` (or prints exact command if `sudo` is required interactively) and displays decoded packets with `tcpdump -nn -X -r capture.pcap`.
  - Robustness comparison scripts: Automated side-by-side runs for each of the 4 robustness mechanisms showing "WITHOUT mechanism" vs. "WITH mechanism" terminal outputs.

## Testing Decisions

### Philosophy
Good tests verify observable external behavior across module boundaries (return codes, wire frames received over sockets, stdout/stderr log output, process table state, and child exit status) rather than internal data structures.

### Seams Under Test
1. **Primary Seam — End-to-End Process & Socket Seam**:
   - Tests spawn the real `server` binary on an ephemeral/test loopback port with specific CLI flags, then drive it using `client`, `evil_client`, or raw socket test harnesses.
   - Verifies:
     - All commands (`sysinfo`, `echo`, `time`, `procs`, `sleep`, `quit`) return valid `RESP_OK` / `RESP_BYE` payloads.
     - 10+ concurrent client connections succeed in parallel and report distinct worker PIDs.
     - Compile-time and runtime log filtering (`LOG_LEVEL=error` vs `LOG_LEVEL=debug`).
     - Each of the 4 robustness mechanisms in both guarded (default) and unguarded (`--no-reap`, `--no-validate`, `--no-timeout`, `--no-limit`) modes.
2. **Secondary Seam — `libnsp.so` Public API Seam**:
   - Unit/integration test binary linked against `libnsp.so` using `socketpair(AF_UNIX, SOCK_STREAM, 0, sv)`.
   - Verifies:
     - `nsp_send_frame` and `nsp_recv_frame` round-trip encoding/decoding and endianness conversion.
     - `nsp_recv_all` and `nsp_send_all` correctly reassemble fragmented writes (e.g., sending 1 byte at a time across the socketpair to simulate TCP partial reads).
     - Header validation rejects bad magic bytes, unsupported versions, and payload lengths exceeding `NSP_MAX_PAYLOAD`.

### Prior Art
- This is a greenfield C codebase for the midterm; earlier weekly homework in `homework/w8/exec_fork.c` used standalone single-file C programs. The CTest suite will establish the project's testing baseline.

## Out of Scope

- Writing the midterm written report narrative (`midterm-report.md` / PDF) and recording the 2-minute video demonstration (the user will author the report and record the video using the provided code and demo scripts).
- Multi-threading (`pthread`), asynchronous event loops (`epoll`/`select` multiplexing multiple clients in a single process), or TLS/encryption.
- Cross-platform Windows/macOS compatibility (targets Linux `/proc` and POSIX sockets specifically, matching the user's Linux environment).
- Persistent database storage or user authentication.

## Further Notes

- Running `ldd build/server build/client` will serve as quick verification in the classroom demo that both binaries dynamically link to `libnsp.so`.
- To ensure `--no-validate` with an oversized length field demonstrates failure safely without triggering the Linux OOM killer on the user's laptop, the unguarded path will attempt `malloc(hdr.payload_len)` (which immediately returns `NULL` for `0xFFFFFFFF` or triggers a controlled abort/segfault simulation in the child worker) and crash/abort the child process with a clear diagnostic message, contrasting cleanly with the guarded mode where the server rejects the frame before allocation and keeps the protocol error path clean.
