# 01: End-to-End Tracer Bullet (CMake, Shared Library Framing, Fork Server, and Client Echo/Quit)

**What to build:** A complete, buildable, end-to-end vertical slice connecting client and server over the custom binary framed TCP protocol. Building the project with CMake produces the shared dynamic library and the server and client executables with embedded RPATH so they run without manual library path variables. The server listens on a configurable TCP port (default 9000), sets address reuse and ignores broken-pipe signals, spawns a dedicated `fork()` child process per client connection (using zero threads), and shuts down cleanly on interrupt/termination signals. The client connects to the server in either single-command mode or interactive REPL mode and can execute `echo <text>`, `help`, and `quit`, exchanging framed 8-byte header packets (`magic 0x4E53`, `version 1`, `msg_type`, `payload_len`) via reliable stream read/write helpers in the shared library.

**Blocked by:** None (can start immediately)

**Status:** ready-for-human

- [x] Project builds cleanly via CMake with strict C11 compiler warnings enabled and produces the shared library alongside the server and client executables with working RPATH (`ldd` confirms dynamic linking to the shared library without setting `LD_LIBRARY_PATH`).
- [x] Shared library implements reliable stream I/O (`send_all` / `recv_all` handling partial transfers and `EINTR`) and binary frame packing/unpacking, verified by automated `socketpair` tests including 1-byte fragmented stream reassembly.
- [x] Server accepts connections and uses `fork()` (no `pthread`) to handle each client in an isolated child process, while setting `SO_REUSEADDR`, ignoring `SIGPIPE`, and catching `SIGINT`/`SIGTERM` for clean shutdown.
- [x] Client supports both non-interactive single-command execution (`-c`) and interactive REPL mode, handling `echo <text>`, `help`, and `quit` (plus EOF) end-to-end against a live server in CTest.

## Comments

### Implementation Summary
- **CMake & Shared Library**: Produced `libnsp.so`, `server`, and `client` with strict C11 (`-Wall -Wextra -Werror -pedantic`) and embedded RPATH (`$ORIGIN`) verified by `ldd`.
- **Reliable Binary Framing**: Implemented 8-byte network byte order headers (`magic 0x4E53`, `version 1`, `msg_type`, `payload_len`) and reliable `nsp_send_all` / `nsp_recv_all` handling `EINTR` and partial stream transfers. Tested with 1-byte fragmented reassembly across `socketpair`.
- **Fork-based Server**: Zero threads (`pthread` absent). Spawns dedicated child worker per client, sets `SO_REUSEADDR`, ignores `SIGPIPE`, catches `SIGINT`/`SIGTERM` for graceful worker shutdown, and reaps zombies immediately via `SIGCHLD` (without `SA_RESTART` blocking).
- **Client & REPL**: Implemented both non-interactive single-command mode (`-c`) and interactive REPL mode, handling `echo`, `help`, `quit`, and EOF cleanly.
- **Future Extensibility**: Command dispatch tables in server and client allow seamless addition of `sysinfo`, `time`, `procs`, and `sleep` for Issue 02. Server configuration struct and logging subsystem lay foundation for Issues 03 and 04. All 3 CTest test suites pass 100%.
