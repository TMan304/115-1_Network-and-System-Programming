# 03: Two-Tier Debug Logging Control (Compile-Time Gate and Runtime Level Filter)

**What to build:** A two-level diagnostic logging system in the shared library used by both client and server. At compile time, a CMake option (`NSP_DEBUG=ON/OFF`) controls whether debug-level log statements are compiled into the binaries or stripped completely by the preprocessor with zero runtime overhead. At runtime, users can set the active log verbosity threshold (`debug`, `info`, `warn`, `error`) via command-line flags (`-l <level>` or `-v`) or the `LOG_LEVEL` environment variable, producing structured log lines with timestamp, process ID, and level tag.

**Blocked by:** 01: End-to-End Tracer Bullet (CMake, Shared Library Framing, Fork Server, and Client Echo/Quit)

**Status:** ready-for-human

- [x] Building with `-DNSP_DEBUG=OFF` strips debug log macro invocations at compile time so that even running with `-l debug` or `LOG_LEVEL=debug` emits no debug-level messages.
- [x] Building with `-DNSP_DEBUG=ON` compiles in debug logging, which remains suppressed at default runtime verbosity (`info`) and appears when enabled via `-l debug`, `-v`, or `LOG_LEVEL=debug`.
- [x] Runtime log threshold accurately filters `debug`, `info`, `warn`, and `error` messages in both client and server processes, and every emitted log line includes timestamp, PID, and level tag.
- [x] Automated tests verify both compile-time stripping behavior and runtime level filtering across levels.

## Comments

### Implementation Summary
1. **Compile-Time Gate (`include/nsp/log.h` & `src/lib/log.c`)**:
   - `LOG_DEBUG(...)` expands to `nsp_log_write(NSP_LOG_DEBUG, __VA_ARGS__)` when `NSP_COMPILE_DEBUG == 1`, and expands to `((void)0)` when `NSP_COMPILE_DEBUG == 0` (zero runtime overhead, arguments and format strings are stripped).
   - In `src/lib/log.c`, added compile-time defense-in-depth: `#if !NSP_COMPILE_DEBUG if (level == NSP_LOG_DEBUG) return; #endif` ensures direct calls to `nsp_log_write(NSP_LOG_DEBUG, ...)` are suppressed even if runtime level is set to `NSP_LOG_DEBUG`.
   - Added `__attribute__((format(printf, 2, 3)))` to `nsp_log_write` for compile-time format string validation.

2. **Runtime Level Filter (`src/lib/log.c` & `src/client/main.c`)**:
   - Supported log levels: `debug` (0), `info` (1), `warn` (2), `error` (3), `off` (4), defaulting to `info`.
   - Supported configuration via `nsp_log_set_level()`, `LOG_LEVEL` environment variable (`nsp_log_init_from_env()`), `-l <level>` / `--log-level <level>`, and `-v` / `--verbose` CLI flags (aliased to `NSP_LOG_DEBUG`).
   - Format conforms strictly to `YYYY-MM-DD HH:MM:SS [PID] [LEVEL] <message>\n` output to `stderr`.
   - Updated `src/client/main.c` with `-v` / `--verbose` support, sets runtime log level via `nsp_log_set_level(cfg.log_level)`, and emits `LOG_DEBUG("Client initialized (host=%s, port=%d)", cfg.host, cfg.port)` for diagnostic observability. Coordinated with Issue 04 implementer for `-v` support in `src/server/main.c`.

3. **Automated Verification (`tests/test_log.c`)**:
   - Verified compile-time stripping: side effects (`++side_effect`) inside `LOG_DEBUG` are verified to NOT evaluate when `NSP_COMPILE_DEBUG == 0` (side effect remains 0) and evaluate when `NSP_COMPILE_DEBUG == 1`.
   - Verified that both direct `nsp_log_write(NSP_LOG_DEBUG, ...)` and `LOG_DEBUG(...)` emit zero bytes when `NSP_COMPILE_DEBUG == 0`.
   - Verified runtime filtering across `info`, `debug`, `warn`, `error`, and `off` levels via `pipe()` / `dup2()` `stderr` capture.
   - Verified POSIX regex and PID conformance for every emitted log line.
   - Verified end-to-end CLI flag and environment variable filtering (`-v`, `--verbose`, `-l error`, `-l off`, `LOG_LEVEL=debug`) by spawning the client binary.
   - Both `build_03_on` (`-DNSP_DEBUG=ON`) and `build_03_off` (`-DNSP_DEBUG=OFF`) compiled cleanly with `-Wall -Wextra -Werror -pedantic` and passed 100% of test suites.
