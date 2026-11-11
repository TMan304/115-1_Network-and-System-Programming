# 03: Two-Tier Debug Logging Control (Compile-Time Gate and Runtime Level Filter)

**What to build:** A two-level diagnostic logging system in the shared library used by both client and server. At compile time, a CMake option (`NSP_DEBUG=ON/OFF`) controls whether debug-level log statements are compiled into the binaries or stripped completely by the preprocessor with zero runtime overhead. At runtime, users can set the active log verbosity threshold (`debug`, `info`, `warn`, `error`) via command-line flags (`-l <level>` or `-v`) or the `LOG_LEVEL` environment variable, producing structured log lines with timestamp, process ID, and level tag.

**Blocked by:** 01: End-to-End Tracer Bullet (CMake, Shared Library Framing, Fork Server, and Client Echo/Quit)

**Status:** ready-for-agent

- [ ] Building with `-DNSP_DEBUG=OFF` strips debug log macro invocations at compile time so that even running with `-l debug` or `LOG_LEVEL=debug` emits no debug-level messages.
- [ ] Building with `-DNSP_DEBUG=ON` compiles in debug logging, which remains suppressed at default runtime verbosity (`info`) and appears when enabled via `-l debug`, `-v`, or `LOG_LEVEL=debug`.
- [ ] Runtime log threshold accurately filters `debug`, `info`, `warn`, and `error` messages in both client and server processes, and every emitted log line includes timestamp, PID, and level tag.
- [ ] Automated tests verify both compile-time stripping behavior and runtime level filtering across levels.
