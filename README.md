# Remote System Monitor Client-Server Application

> **Course:** 115-1 Network and System Programming (Midterm Project)  
> **Architecture:** Pure `fork()` Multi-Process UNIX Architecture (Strictly Zero Threads)  
> **Shared Library:** Dynamic Library `libnsp.so` with Embedded RPATH  
> **Protocol:** Binary Framed TCP Protocol (`0x4E53` / `"NS"`)  
> **Build System:** CMake (C11, `-Wall -Wextra -Werror -pedantic`)

---

## Table of Contents

- [Remote System Monitor Client-Server Application](#remote-system-monitor-client-server-application)
  - [Table of Contents](#table-of-contents)
  - [Project Overview](#project-overview)
    - [Key Highlights](#key-highlights)
  - [Midterm Requirements Compliance Matrix](#midterm-requirements-compliance-matrix)
  - [System Architecture \& Process Model](#system-architecture--process-model)
    - [Process Lifecycle \& Isolation](#process-lifecycle--isolation)
  - [Wire Protocol Specification](#wire-protocol-specification)
    - [Header Fields](#header-fields)
  - [Building the Project](#building-the-project)
    - [Prerequisites](#prerequisites)
    - [Debug Build (Default)](#debug-build-default)
    - [Release Build (Optimized \& Debug Strings Stripped)](#release-build-optimized--debug-strings-stripped)
    - [Verifying Dynamic Linking (`ldd`)](#verifying-dynamic-linking-ldd)
  - [Running the Application](#running-the-application)
    - [Server (`server`)](#server-server)
    - [Standard Client (`client`)](#standard-client-client)
      - [Interactive REPL Mode](#interactive-repl-mode)
      - [Single-Command Non-Interactive Mode](#single-command-non-interactive-mode)
    - [Adversarial Testing Client (`evil_client`)](#adversarial-testing-client-evil_client)
  - [Demonstration Scripts (Classroom Demo \& Video Recording)](#demonstration-scripts-classroom-demo--video-recording)
    - [Script 1: 10+ Concurrent Clients Runtime Proof](#script-1-10-concurrent-clients-runtime-proof)
    - [Script 2: Packet Capture \& Wire Protocol Analysis](#script-2-packet-capture--wire-protocol-analysis)
    - [Script 3: Four Robustness Mechanisms A/B Demonstrations](#script-3-four-robustness-mechanisms-ab-demonstrations)
      - [Robustness Comparison Summary Table](#robustness-comparison-summary-table)
    - [Script 4: Two-Tier Debug Logging Demonstration](#script-4-two-tier-debug-logging-demonstration)
    - [Master Runner: All-in-One Demo Suite](#master-runner-all-in-one-demo-suite)
  - [2-Minute Demonstration Video Guide](#2-minute-demonstration-video-guide)
    - [Suggested Video Timing (Total: 120 Seconds)](#suggested-video-timing-total-120-seconds)
  - [Automated Testing Suite (CTest)](#automated-testing-suite-ctest)
    - [Test Cases](#test-cases)

---

## Project Overview

This project implements a high-performance, robust, C-based client-server remote system monitoring system for Linux. The server dynamically inspects host kernel metrics (uptime, CPU model and cores, memory utilization, load average, system processes) and serves multiple clients simultaneously over a custom framed binary TCP wire protocol.

### Key Highlights

- **Strict Process-Based Concurrency:** Concurrency is achieved exclusively via `fork()`. Each client connection is handled by an isolated worker child process. **POSIX threads (`pthread`) are strictly prohibited and completely absent from the codebase.**
- **Encapsulated Dynamic Shared Library (`libnsp.so`):** Protocol encoding/decoding, reliable stream I/O (`nsp_send_all` / `nsp_recv_all` with partial read/write loops and `EINTR` retry), and logging are compiled into a shared library. Executables embed an RPATH of `$ORIGIN` to run out-of-the-box without configuring `LD_LIBRARY_PATH`.
- **Four Toggleable Robustness Mechanisms:** Real-world failure modes (zombie accumulation, malformed frames, slow-loris idle sockets, and connection floods) are prevented by default and can be toggled via CLI flags for live before/after A/B classroom demonstrations.
- **Two-Tier Logging Control:** Compile-time preprocessor gate (`NSP_COMPILE_DEBUG`) eliminates debug strings in production, while runtime filters (`-l` / `LOG_LEVEL`) dynamically adjust log verbosity.

---

## Midterm Requirements Compliance Matrix

| Requirement from Course Spec (`docs/midterm.md`) | Implementation & Source Location | Verification / Demonstration Script |
| :--- | :--- | :--- |
| **1. Server uses `fork()` for each client (no threads)** | [src/server/server.c](file:///home/tman304/Projects/115-1_Network-and-System-Programming/src/server/server.c) (`fork()` call in accept loop) | [scripts/demo_concurrency.sh](file:///home/tman304/Projects/115-1_Network-and-System-Programming/scripts/demo_concurrency.sh), `ps --forest`, `grep -rn "pthread" src/` |
| **2. Handle >= 10 concurrent clients with runtime proof** | [src/server/server.c](file:///home/tman304/Projects/115-1_Network-and-System-Programming/src/server/server.c), [src/server/handlers.c](file:///home/tman304/Projects/115-1_Network-and-System-Programming/src/server/handlers.c) | [scripts/demo_concurrency.sh](file:///home/tman304/Projects/115-1_Network-and-System-Programming/scripts/demo_concurrency.sh) (12 concurrent clients, `ps --forest`, `ss -tnp`) |
| **3. Packet capture & analysis (`tcpdump` / Wireshark)** | [include/nsp/protocol.h](file:///home/tman304/Projects/115-1_Network-and-System-Programming/include/nsp/protocol.h), [src/lib/protocol.c](file:///home/tman304/Projects/115-1_Network-and-System-Programming/src/lib/protocol.c) | [scripts/demo_packet_capture.sh](file:///home/tman304/Projects/115-1_Network-and-System-Programming/scripts/demo_packet_capture.sh) (`tcpdump -nn -X -r nsp_traffic.pcap`) |
| **4. Two-tier debug logging (compile-time & runtime)** | [include/nsp/log.h](file:///home/tman304/Projects/115-1_Network-and-System-Programming/include/nsp/log.h), [src/lib/log.c](file:///home/tman304/Projects/115-1_Network-and-System-Programming/src/lib/log.c) | [scripts/demo_logging.sh](file:///home/tman304/Projects/115-1_Network-and-System-Programming/scripts/demo_logging.sh) (`-DNSP_DEBUG` + `-l <level>`) |
| **5. Shared dynamic library (`.so`) linked by client & server** | [src/lib/](file:///home/tman304/Projects/115-1_Network-and-System-Programming/src/lib/) -> `libnsp.so` | `ldd build/server build/client build/evil_client` |
| **6. Client requests system info from server** | [src/server/handlers.c](file:///home/tman304/Projects/115-1_Network-and-System-Programming/src/server/handlers.c) (`sysinfo`, `uname`, `/proc`) | `./build/client -c "sysinfo"` |
| **7. Robustness mechanisms for abnormal conditions** | [src/server/server.c](file:///home/tman304/Projects/115-1_Network-and-System-Programming/src/server/server.c), [src/lib/protocol.c](file:///home/tman304/Projects/115-1_Network-and-System-Programming/src/lib/protocol.c) | 4 toggleable mechanisms: Reap, Validate, Timeout, Limit |
| **8. At least 3 A/B execution examples (with vs. without)** | [scripts/demo_robustness.sh](file:///home/tman304/Projects/115-1_Network-and-System-Programming/scripts/demo_robustness.sh), [src/evil_client/](file:///home/tman304/Projects/115-1_Network-and-System-Programming/src/evil_client/) | Demonstrates **all 4 mechanisms** with clear side-by-side terminal output |
| **9. Built using Makefile or CMake** | [CMakeLists.txt](file:///home/tman304/Projects/115-1_Network-and-System-Programming/CMakeLists.txt) | `cmake -B build && cmake --build build` |

---

## System Architecture & Process Model

```markdown
                           +----------------------------------------+
                           |         Parent Server Listener         |
                           |   - accept() TCP client connections    |
                           |   - Tracks active child PIDs           |
                           |   - SIGCHLD waitpid(-1, WNOHANG) loop  |
                           |   - Admission limit (-m <max>)         |
                           +-------------------+--------------------+
                                               |
                     +-------------------------+-------------------------+
                     | fork()                  | fork()                  | fork()
                     v                         v                         v
           +-------------------+     +-------------------+     +-------------------+
           |  Child Worker #1  |     |  Child Worker #2  |     |  Child Worker #N  |
           | PID: 21691        |     | PID: 21695        |     | PID: 21703        |
           | Close listen fd   |     | Close listen fd   |     | Close listen fd   |
           | Set SO_RCVTIMEO   |     | Set SO_RCVTIMEO   |     | Set SO_RCVTIMEO   |
           | Serves Client #1  |     | Serves Client #2  |     | Serves Client #N  |
           +-------------------+     +-------------------+     +-------------------+
                     ^                         ^                         ^
                     | TCP                     | TCP                     | TCP
                     v                         v                         v
           +-------------------+     +-------------------+     +-------------------+
           |     Client #1     |     |     Client #2     |     |     Client #N     |
           +-------------------+     +-------------------+     +-------------------+

                     =============================================
                      Linked dynamically to libnsp.so (RPATH: $ORIGIN)
                     =============================================
```

### Process Lifecycle & Isolation

1. **Parent Listener:** Creates a stream socket, applies `SO_REUSEADDR`, suppresses `SIGPIPE` (`signal(SIGPIPE, SIG_IGN)`), installs a `SIGCHLD` reaping handler, and binds to the target port.
2. **Accept & Admission Control:** When a client arrives:
   - If the active child worker count reaches `max_clients` (and `--no-limit` is not active), the parent immediately transmits a `RESP_BUSY` frame and closes the socket without forking.
   - Otherwise, `fork()` is invoked.
3. **Dedicated Worker Process:**
   - Immediately closes the parent's listening socket.
   - Applies the socket receive timeout (`SO_RCVTIMEO`).
   - Runs a framed request/response loop until EOF, `REQ_QUIT`, or timeout occurs.
   - Closes the connection and calls `exit(0)`.
4. **Parent Reaping:** Upon worker termination, the kernel delivers `SIGCHLD`. The signal handler executes `while ((pid = waitpid(-1, &status, WNOHANG)) > 0)` to reap all terminated children immediately without blocking, decrementing the active connection counter.

---

## Wire Protocol Specification

NSP uses a custom binary framed stream protocol. Every message begins with an 8-byte fixed header in network byte order (big-endian), followed immediately by the payload:

```markdown
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Magic (0x4E53 = "NS")   | Version(0x01) | Msg Type      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                 Payload Length (32-bit Big-Endian)            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Payload Bytes (len bytes) ...           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### Header Fields

- **Magic (`uint16_t`, 2 bytes):** Constant `0x4E53` (ASCII `"NS"`). Makes packets immediately identifiable in `tcpdump` and Wireshark.
- **Version (`uint8_t`, 1 byte):** Protocol version constant `0x01`.
- **Msg Type (`uint8_t`, 1 byte):**
  - **Client Requests:**
    - `0x01` (`REQ_SYSINFO`): Request host system information.
    - `0x02` (`REQ_ECHO`): Echo request (payload echoed back verbatim).
    - `0x03` (`REQ_TIME`): Request epoch and formatted server timestamp.
    - `0x04` (`REQ_PROCS`): Request worker PID, parent PID, active and system processes.
    - `0x05` (`REQ_SLEEP`): Instruct worker child to pause for `<sec>` seconds.
    - `0x06` (`REQ_QUIT`): Cleanly terminate session.
  - **Server Responses:**
    - `0x80` (`RESP_OK`): Successful command execution.
    - `0x81` (`RESP_ERROR`): Protocol error, invalid syntax, or timeout message.
    - `0x82` (`RESP_BUSY`): Server connection limit exceeded.
    - `0x83` (`RESP_BYE`): Farewell acknowledgement for `REQ_QUIT`.
- **Payload Length (`uint32_t`, 4 bytes):** Length of payload in bytes (maximum 4096 bytes under standard validation).

---

## Building the Project

### Prerequisites

- Linux OS (Ubuntu 20.04+, Debian 11+, Arch, etc.)
- GCC or Clang supporting C11
- CMake 3.16+
- standard UNIX utilities (`ps`, `ss`, `tcpdump`, `python3`)

### Debug Build (Default)

Enables compile-time debug logging (`LOG_DEBUG` statements active):

```bash
cmake -B build -DNSP_DEBUG=ON
cmake --build build
```

### Release Build (Optimized & Debug Strings Stripped)

Strips all `LOG_DEBUG` statements at the preprocessor level:

```bash
cmake -B build-release -DNSP_DEBUG=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

### Verifying Dynamic Linking (`ldd`)

To verify that `server`, `client`, and `evil_client` dynamically link to `libnsp.so` with embedded RPATH:

```bash
ldd build/server build/client build/evil_client
```

Expected output:

```text
build/server:
        libnsp.so => ./libnsp.so (0x...)
        libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6 (0x...)
build/client:
        libnsp.so => ./libnsp.so (0x...)
...
```

---

## Running the Application

### Server (`server`)

```text
Usage: ./build/server [OPTIONS]
Options:
  -p, --port <port>       Listening port (default: 9000)
  -m, --max-clients <num> Maximum concurrent client connections (default: 16)
  -t, --timeout <sec>     Client idle socket receive timeout (default: 30)
  -l, --log-level <lvl>   Log level (debug, info, warn, error, off)
  -v, --verbose           Enable debug logging (alias for -l debug)
  --no-reap               Disable SIGCHLD zombie reaping (Robustness #1 demo)
  --no-validate           Disable binary frame header validation (Robustness #2 demo)
  --no-timeout            Disable socket receive timeout (Robustness #3 demo)
  --no-limit              Disable connection limit check (Robustness #4 demo)
  --no-robust             Disable all four robustness mechanisms simultaneously
  -h, --help              Show help message
```

### Standard Client (`client`)

The client can be run interactively or in non-interactive batch mode:

#### Interactive REPL Mode

```bash
./build/client -p 9000
```

Available interactive commands:

- `sysinfo` — Displays remote OS, uptime, load average, CPU model/cores, memory.
- `echo <text>` — Echoes `<text>` back.
- `time` — Displays server epoch timestamp and ISO local/UTC time.
- `procs` — Displays assigned worker PID, parent server PID, active clients, system procs.
- `sleep <1-60>` — Holds connection for `<sec>` seconds to demonstrate concurrency.
- `help` — Lists commands.
- `quit` — Clean session shutdown.

#### Single-Command Non-Interactive Mode

```bash
./build/client -p 9000 -c "sysinfo"
./build/client -p 9000 -c "echo Hello from CLI"
./build/client -p 9000 -c "procs"
```

### Adversarial Testing Client (`evil_client`)

```text
Usage: ./build/evil_client [OPTIONS] [MODE]
Options:
  -p, --port <port>       Target server port (default: 9000)
  --hold <ms>             Connection hold time in ms for --flood (default: 500)
Modes:
  --bad-magic             Send header with invalid magic (0xDEAD)
  --oversized             Send header with oversized payload length (0x7FFFFFFF)
  --truncated             Send header claiming 64B payload, send only 4B, and close write half
  --half-open             Send 3 bytes of partial header and abruptly disconnect
  --idle [sec]            Open silent connection and wait up to [sec] seconds (default: 3)
  --flood <count>         Open <count> simultaneous connections to test connection limit
  --zombie-burst <count>  Open <count> rapid sequential quit connections
```

---

## Demonstration Scripts (Classroom Demo & Video Recording)

All demonstration scripts are self-contained, colorized, and suitable for live presentation and screen recording.

### Script 1: 10+ Concurrent Clients Runtime Proof

Demonstrates 12 simultaneous client connections, displays `ps --forest` (parent-child tree) and `ss -tnp` (12 TCP connections), and verifies 12 distinct worker PIDs:

```bash
./scripts/demo_concurrency.sh
```

### Script 2: Packet Capture & Wire Protocol Analysis

Captures loopback traffic into a `.pcap` file using `tcpdump`, and prints decoded hex/ASCII dumps highlighting `"NS"` (`0x4E53`), version, message types, and lengths:

```bash
# Run packet inspection (creates nsp_traffic.pcap and decodes via tcpdump -nn -X -r)
./scripts/demo_packet_capture.sh

# Or run with sudo for live network interface capture:
sudo ./scripts/demo_packet_capture.sh
```

### Script 3: Four Robustness Mechanisms A/B Demonstrations

Demonstrates side-by-side execution ("WITHOUT mechanism" vs. "WITH mechanism") for all 4 robustness mechanisms:

```bash
# Run all 4 demonstrations:
./scripts/demo_robustness.sh all

# Or run individual demonstrations:
./scripts/demo_robustness.sh 1    # Zombie Reaping (shows <defunct> vs clean reap)
./scripts/demo_robustness.sh 2    # Frame Validation (shows crash/abort vs clean RESP_ERROR)
./scripts/demo_robustness.sh 3    # Idle Timeout (shows hung socket vs 2s timeout)
./scripts/demo_robustness.sh 4    # Connection Limit (shows unconstrained forks vs RESP_BUSY)
```

#### Robustness Comparison Summary Table

| Robustness Mechanism | Threat / Failure Mode in Naive Servers | Behavior WITHOUT Mechanism | Behavior WITH Mechanism (Default) |
| :--- | :--- | :--- | :--- |
| **#1: Zombie Process Reaping** | Child processes exit without `waitpid()`, creating `<defunct>` zombies that exhaust OS process table slots. | **`--no-reap`:** Terminated workers linger as `<defunct>` (State `Z`) in `ps`. | **Default:** Handled via non-blocking `waitpid(-1, WNOHANG)` loop in `SIGCHLD`. Zero zombies remain. |
| **#2: Frame Validation** | Malicious / corrupt packets send invalid magic or 2 GiB length fields causing integer overflow or crash. | **`--no-validate`:** Worker attempts unchecked allocation / read, fails or terminates abruptly. | **Default:** Validates magic `0x4E53`, version `0x01`, and caps length to 4 KiB. Returns clean `RESP_ERROR`. |
| **#3: Client Idle Timeout** | Silent / abandoned clients hold worker processes and sockets open indefinitely (Slowloris attack). | **`--no-timeout`:** Worker blocks on `recv()` forever, keeping child process active in `ps`. | **Default (`-t <sec>`):** Sockets apply `SO_RCVTIMEO`. Times out, logs warning, returns `RESP_ERROR`, and exits cleanly. |
| **#4: Connection Limit** | Connection flood / burst attacks exhaust system CPU and process limits by spawning unlimited child workers. | **`--no-limit`:** Server forks child processes for all incoming connections without bound. | **Default (`-m <max>`):** Tracks active children. Excess clients receive `RESP_BUSY` and are closed without calling `fork()`. |

### Script 4: Two-Tier Debug Logging Demonstration

Demonstrates both compile-time preprocessor gating (`NSP_COMPILE_DEBUG`) and dynamic runtime verbosity filtering:

```bash
./scripts/demo_logging.sh
```

### Master Runner: All-in-One Demo Suite

Runs all demonstrations sequentially with interactive prompts between sections (ideal for classroom demonstrations and recording the 2-minute video):

```bash
# Interactive mode (pauses between demonstrations):
./scripts/run_all_demos.sh

# Batch mode (non-interactive):
./scripts/run_all_demos.sh --batch
```

---

## 2-Minute Demonstration Video Guide

To record the 2-minute video required by the course, open a terminal window (80x24 or larger) and execute:

```bash
./scripts/run_all_demos.sh
```

### Suggested Video Timing (Total: 120 Seconds)

1. **0:00 - 0:20 (20s) — Linking & Architecture:**
   - Display `ldd build/server build/client` showing dynamic linking to `libnsp.so`.
   - Mention pure `fork()` process model (zero threads).
2. **0:20 - 0:50 (30s) — 10+ Concurrent Clients Runtime Proof:**
   - Run `demo_concurrency.sh`.
   - Highlight the `ps --forest` tree showing 12 worker processes and `ss -tnp` showing 12 connections.
   - Point out the verification table showing 12 distinct worker PIDs.
3. **0:50 - 1:15 (25s) — Packet Capture & Protocol Inspection:**
   - Run `demo_packet_capture.sh`.
   - Point to `0x4E53` magic header `"NS"`, message type, length field, and ASCII payload in `tcpdump -nn -X -r`.
4. **1:15 - 1:45 (30s) — Robustness A/B Demonstration:**
   - Run `demo_robustness.sh`.
   - Show Zombie Reaping: `<defunct>` zombies in `--no-reap` vs. 0 zombies in default.
   - Show Connection Limit: rejection with `RESP_BUSY` when exceeding `-m 3`.
5. **1:45 - 2:00 (15s) — Conclusion & Test Suite:**
   - Show `ctest` running all 6 automated tests passing at 100%.

---

## Automated Testing Suite (CTest)

The project includes an end-to-end automated test suite registered with CMake CTest:

```bash
ctest --test-dir build --output-on-failure
```

### Test Cases

1. `test_framing`: Unit tests for protocol framing, endianness conversion, fragmented stream assembly (`socketpair`), and header validation.
2. `test_log`: Unit tests for log level thresholds, formatting, and preprocessor gate.
3. `test_e2e_echo_quit`: End-to-end client-server integration tests for `echo` and `quit`.
4. `test_sysinfo_concurrency`: End-to-end integration tests for `sysinfo`, `time`, `procs`, `sleep`, and 12-client concurrency.
5. `test_robustness`: End-to-end integration tests for all 4 robustness mechanisms (zombie reaping, validation, idle timeout, connection limit).
6. `test_demo_scripts`: Automated verification that all demonstration shell scripts execute and pass without error.
