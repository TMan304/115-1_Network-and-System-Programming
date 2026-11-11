# 02: System Monitoring and Concurrency Commands (sysinfo, time, procs, sleep)

**What to build:** Full remote system monitoring and concurrency inspection capabilities between client and server. Users can issue `sysinfo` to retrieve the server's hostname, OS/kernel release, uptime, 1/5/15-minute load averages, CPU model and core count, and total/available memory; `time` to query the server's current epoch and formatted local/UTC clock; `procs` to inspect the dedicated worker child PID, parent server PID, active concurrent client count, and total system process count; and `sleep <seconds>` to hold a worker connection open for concurrency testing.

**Blocked by:** 01: End-to-End Tracer Bullet (CMake, Shared Library Framing, Fork Server, and Client Echo/Quit)

**Status:** ready-for-agent

- [ ] `sysinfo` command returns a structured report containing non-empty hostname, kernel/OS version, uptime, load averages, CPU model/core count, and memory statistics gathered from standard Linux interfaces.
- [ ] `time` command returns the server's current timestamp and formatted time string.
- [ ] `procs` command returns the serving child's PID (`getpid()`), parent server PID (`getppid()`), active concurrent client count, and system process count.
- [ ] `sleep <seconds>` pauses the serving child worker for the requested duration (within safe bounds) and replies with the worker PID and elapsed time.
- [ ] Automated end-to-end test launches at least 10 concurrent clients holding overlapping connections and verifies that all 10 succeed simultaneously and are served by distinct child worker PIDs.
