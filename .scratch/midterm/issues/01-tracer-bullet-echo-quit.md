# 01: End-to-End Tracer Bullet (CMake, Shared Library Framing, Fork Server, and Client Echo/Quit)

**What to build:** A complete, buildable, end-to-end vertical slice connecting client and server over the custom binary framed TCP protocol. Building the project with CMake produces the shared dynamic library and the server and client executables with embedded RPATH so they run without manual library path variables. The server listens on a configurable TCP port (default 9000), sets address reuse and ignores broken-pipe signals, spawns a dedicated `fork()` child process per client connection (using zero threads), and shuts down cleanly on interrupt/termination signals. The client connects to the server in either single-command mode or interactive REPL mode and can execute `echo <text>`, `help`, and `quit`, exchanging framed 8-byte header packets (`magic 0x4E53`, `version 1`, `msg_type`, `payload_len`) via reliable stream read/write helpers in the shared library.

**Blocked by:** None (can start immediately)

**Status:** ready-for-agent

- [ ] Project builds cleanly via CMake with strict C11 compiler warnings enabled and produces the shared library alongside the server and client executables with working RPATH (`ldd` confirms dynamic linking to the shared library without setting `LD_LIBRARY_PATH`).
- [ ] Shared library implements reliable stream I/O (`send_all` / `recv_all` handling partial transfers and `EINTR`) and binary frame packing/unpacking, verified by automated `socketpair` tests including 1-byte fragmented stream reassembly.
- [ ] Server accepts connections and uses `fork()` (no `pthread`) to handle each client in an isolated child process, while setting `SO_REUSEADDR`, ignoring `SIGPIPE`, and catching `SIGINT`/`SIGTERM` for clean shutdown.
- [ ] Client supports both non-interactive single-command execution (`-c`) and interactive REPL mode, handling `echo <text>`, `help`, and `quit` (plus EOF) end-to-end against a live server in CTest.
