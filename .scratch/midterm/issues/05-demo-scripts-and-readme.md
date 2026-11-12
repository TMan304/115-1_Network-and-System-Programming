# 05: Live Demo Scripts (10+ Concurrency, tcpdump Capture, Robustness A/B) and README

**What to build:** Self-contained demonstration scripts and concise documentation so the user can effortlessly run classroom live demonstrations, capture screenshots, and record the 2-minute demo video. Includes automated scripts for the 10+ concurrent client runtime proof, loopback packet capture and analysis with `tcpdump`, two-tier logging demonstration, and side-by-side "with vs. without" execution of the four robustness mechanisms.

**Blocked by:** 02: System Monitoring and Concurrency Commands (sysinfo, time, procs, sleep), 03: Two-Tier Debug Logging Control (Compile-Time Gate and Runtime Level Filter), 04: Four Toggleable Robustness Mechanisms and Adversarial Client (evil_client)

**Status:** resolved

- [x] Concurrency demo script starts the server, spawns 12 simultaneous clients holding connections via `sleep`, captures runtime proof using `ps --forest` and `ss -tnp`, and displays each client's distinct worker PID response.
- [x] Packet capture script captures loopback traffic (`tcpdump -i lo`) into a `.pcap` file during a sample session and prints decoded packet headers/payloads (`tcpdump -nn -X -r`) highlighting the `"NS"` (`0x4E53`) magic header, message type, and length fields.
- [x] Robustness demo script runs clear before/after ("WITHOUT mechanism" vs. "WITH mechanism") demonstrations for at least three (and up to all four) robustness mechanisms with readable terminal banners suitable for screenshots and video recording.
- [x] Project documentation explains how to build (release vs. debug), run CTest, link/inspect `libnsp.so` with `ldd`, and run each demonstration script.
