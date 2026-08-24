# DNC Serial Engine

[![build](https://github.com/dev-kaiki/dnc-serial-engine/actions/workflows/build.yml/badge.svg)](https://github.com/dev-kaiki/dnc-serial-engine/actions/workflows/build.yml)

An RS-232 **DNC engine** (Direct Numerical Control) for CNC machine tools, written in C++17 / Qt.
It streams G-code programs to CNC controllers over serial, receives programs back from them, and — critically — does so **without overflowing the machine's receive buffer**, on hardware that gives you almost no feedback to work with.

Extracted from a system in daily use on a real shop floor.
Built and maintained by [Kaiki Quadros Ferreira](https://github.com/dev-kaiki) at SMI, published with the company's permission.
Ported to Android tablets in [`dnc-android`](https://github.com/dev-kaiki/dnc-android), which carries this engine over unchanged behind an extracted `ISerialPort` interface.

---

## The problem this solves

A CNC controller from the 80s or 90s has a serial port, a few kilobytes of buffer, and no way to tell you how full that buffer is. You get one bit of backpressure — `XOFF`, or `CTS` going low — and it arrives *late*.

Send too fast and the machine silently drops characters in the middle of a program. On a machine cutting steel, a dropped block is a crashed tool at best.
Send too slow and a 40-minute job becomes a 4-hour one, and drip-feed (streaming a program larger than the machine's memory, in real time, while it cuts) stops being viable at all.

The engine has to sit exactly between those two failure modes, on machines that all behave differently, using a protocol that tells it almost nothing.

## Approach

Flow control is treated as a **control loop**, not a fixed configuration:

- **Adaptive chunking** — transmission chunk size grows while the machine keeps draining, and downshifts aggressively the moment `XOFF` or `CTS`-low arrives early. Growth only resumes after a stable volume has passed.
- **Strict pacing mode** — when the operator sets an explicit byte rate, the scheduler switches to deterministic micro-bursts instead of adaptive growth. Two distinct strategies, never blended.
- **Driver-native flow control when available** — `XON/XOFF` and `RTS/CTS` are delegated to the OS driver where the profile allows it, rather than reimplemented on top of a scheduler that is already throttling.
- **Bounded write buffers** — `QSerialPort`'s write buffer is capped deliberately. A large OS-side buffer hides the true state of the wire and makes overshoot invisible until it's too late.
- **Watchdog with mode awareness** — the no-progress watchdog is disabled when flow control is driver-native, to avoid false holds.

## Architecture

```
DncEngine ──── orchestration, session lifecycle, statistics
   │
   ├── StateMachine ........ explicit session states and legal transitions
   ├── ProgramSource ....... G-code source, framing, block boundaries
   ├── TxScheduler ......... adaptive chunk sizing / strict pacing
   ├── FlowController ...... XON/XOFF, RTS/CTS, DTR/DSR decision logic
   ├── HandshakeMonitor .... line-signal observation and transitions
   ├── DrainController ..... end-of-send drain before declaring completion
   ├── Watchdog ............ stall and inactivity detection
   ├── RxInterpreter ....... inbound stream parsing, receive termination
   │
   ├── SerialPortFacade .... QSerialPort wrapper, buffer and signal control
   ├── CommDiagnostics ..... link diagnostics
   ├── CncSimulator ........ bench-testable CNC stand-in
   └── SessionTraceRepository / HistoryRepository .... per-session trace and history
```

Every component is independently testable. `Result<T>` is used for fallible operations instead of exceptions.

## Engineering log

[`docs/engineering-log/`](docs/engineering-log/) documents the real debugging history of the flow-control engine — patch by patch, in the order the problems were actually found on live machines. Each entry states the **root cause attacked**, the change made, the expected effect, and the **known limitation** that remains.

A representative sequence:

| Patch | Root cause | Outcome |
|---|---|---|
| `V8` | Non-deterministic RTS/CTS handling | Manual RTS control, drain must confirm all bytes accepted before completing |
| `V30` | Buffer overflow on small-buffer CNCs | Strict pacing via 1–4 byte micro-bursts; capped driver write buffer |
| `V31` | V30's fix imposed an artificial ~1400 B/s ceiling | Split adaptive flow from strict pacing; faster chunk growth under real backpressure |
| `V32B` | "Starts fine, chokes mid-program" | Slower chunk growth, tighter high-water marks, longer post-`XON` recovery window |

The `V30 → V31 → V32B` sequence is the honest part: fixing the overflow created a throughput ceiling, fixing the ceiling reintroduced mid-program choking, and the third pass found the balance. The docs say so plainly, including what the engine still *cannot* know because the cable never reports it.

## Building

Requires Qt 5 or Qt 6 (`Core`, `SerialPort`; `Test` for unit tests) and CMake 3.16+.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

## Testing without a CNC

`CncSimulator` emulates a controller with a bounded buffer and configurable `XON/XOFF` behaviour, so the flow logic can be exercised on a bench.

[`tests/serial/`](tests/serial/) contains loopback and `XON/XOFF` simulation scripts for testing against a real serial port pair.

## Scope of this repository

This is the **engine core**. The desktop application shell, the installer, and the licensing subsystem that surround it in the commercial product are not included here — they are SMI's, and they are not what makes this interesting.

## License

MIT — see [LICENSE](LICENSE). Published with permission from SMI.
