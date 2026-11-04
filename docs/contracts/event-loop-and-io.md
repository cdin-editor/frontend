# Event loop, readiness and I/O ownership

This document defines the required runtime behavior and separates the TUI
readiness coordinator from Core Protocol I/O. Spec FE-019 requires a
readiness-driven TUI loop without busy-waiting. The transport-agnostic
coordinator and additive incremental Protocol client now exist and are tested
with fake sources; the platform wait bridge and live composition-root wiring
remain missing.

## 1. Ownership

- The Frontend adapter owns its UI event loop and device readiness.
- The Core Protocol client owns framing, CBOR validation, sequence checks and
  bounded incremental message state.
- The `cdin` composition root owns the concrete transport endpoint and wires
  its readiness callback/handle into the selected Frontend.
- The Backend owns the Runtime owner thread and is the only component that
  calls Runtime/Lua and the Kernel adapter.

The Frontend event loop never calls Lua, Kernel APIs or Backend private
functions. It sends messages through the Frontend Protocol client.

## 2. Why the current API is insufficient

The legacy `cdin_frontend_protocol_io` contains blocking `read` and `write`
callbacks; `cdin_frontend_protocol_read_frame()` reads one whole frame. The
additive `async_client.h` API accepts nonblocking callbacks with explicit
`PROGRESS`, `AGAIN`, EOF and failure results. The
current TUI terminal adapter has `read`, `write` and `size`, but no readiness
wait. If the TUI blocks waiting for Protocol input, it cannot react to terminal
input/resize. If it blocks waiting for terminal input, it cannot receive
Backend messages or ACK pressure. A sleep loop or repeated nonblocking reads
would become a busy-wait and is forbidden.

## 3. Implemented TUI coordinator

`ui/tui/include/cdin/frontend/ui/tui/event_loop.h` and
`ui/tui/src/session/event_loop.c` provide a transport-agnostic coordinator. The
composition root supplies a `wait` callback that blocks until terminal input,
Backend transport, resize or termination is ready. The coordinator then:

- services both terminal and Backend when both are ready in the same turn;
- gives each source a bounded item budget and requires explicit progress,
  `AGAIN`, EOF or error results;
- dispatches resize and termination through callbacks;
- renders after Backend updates or geometry changes;
- calls the shutdown callback on terminal/backend closure and errors.

The coordinator passes a fake readiness test. It does not implement OS waits,
protocol partial reads/writes, signal handlers, terminal cleanup itself or a
runnable editor. The `cdin` composition root must own wait-set construction and
must call terminal restoration from its shutdown callback.

## 4. Required loop behavior

```text
initialize terminal/window adapter
initialize protocol client and session
send hello; receive hello_ack; validate version/capabilities
send attach; wait until attached

while running:
    drain already-buffered Protocol frames
    stage inbound messages until ui.flush
    validate and atomically commit completed flush groups
    ACK only successfully committed flush groups
    discard stale line data and request visible ranges for current versions
    drain available terminal/SDL events
    translate physical input into Protocol messages (driver only)
    render only when committed model or presentation state is dirty
    wait for terminal/window OR transport readiness OR shutdown/resize

send detach when possible
stop accepting new events
release client/model resources
restore terminal/window state
```

The loop must not call a blocking full-frame read while waiting for device
input. It must handle simultaneous readiness and process bounded work per
iteration so neither input nor Backend updates starve.

## 5. Proposed Core I/O contract

The exact public symbols are not frozen. The proposed contract must separate
progress, temporary unavailability, orderly closure and fatal error. A
suitable result model is:

| Result | Meaning |
| --- | --- |
| `PROGRESS` | Some bytes were read/written; count is non-zero. |
| `AGAIN` | No progress now; keep the connection open and wait for readiness. |
| `EOF` | Peer closed the read side cleanly. |
| `ERROR` | Fatal I/O failure; details are provided to diagnostics. |

A proposed readiness mask has at least `READABLE`, `WRITABLE`, `RESIZE` and
`TERMINATE`. The wait operation accepts a timeout/deadline so the adapter can
resolve an ambiguous ESC key without polling. Timeout means no readiness event
arrived; it is not EOF or an error.

The next API revision should:

- use explicit struct-size/version fields for public callback tables;
- allow partial reads and writes and retain exact offsets in pending frames;
- never treat `AGAIN` as EOF;
- never busy-loop after `AGAIN`;
- keep memory bounds for pending read/write frames;
- keep the framing/codec independent from the operating system;
- make a wait/poll integration possible without exposing `pollfd`, SDL or
  Windows HANDLE types in Core public headers.

This is a local proposal, not an accepted stable ABI. See
[FE-ADR-0001](../proposals/FE-ADR-0001-readiness-driven-event-loop.md).

## 6. Platform integration

### POSIX TUI

Use a readiness primitive such as `poll()`/`ppoll()` on terminal input and the
composition-root transport descriptor/event handle. Retry `EINTR` after
checking the termination flag; surface `EAGAIN` as not-ready; distinguish EOF
from a zero-timeout. Signal handlers only set a `sig_atomic_t` flag. Terminal
restore and Backend detach occur on the normal owner thread.

### Windows TUI

Use console input/event handles and the wait primitive available to the
composition-root transport. Preserve and restore the original console input
and output modes. Keep `windows.h` and console structures inside the TUI
platform adapter.

### SDL GUI

SDL events and transport readiness must be multiplexed without starving either
side. The GUI adapter can use SDL's wait/event mechanism plus a transport
readiness bridge owned by the composition root. The Frontend must not create
an OS-specific Backend transport in Core.

### Headless

The Headless adapter can use deterministic in-memory streams and scripted
readiness. It should support deterministic timeout/EOF/error simulation so
client state transitions are testable without wall-clock sleeps.

## 7. Terminal teardown invariant

The TUI stores the exact terminal state before changing it. It restores raw
mode, cursor visibility/position, alternate screen, bracketed paste and any
other terminal modes it enabled on:

- normal quit;
- attach failure after terminal acquisition;
- protocol/transport failure;
- rendering or write error;
- EOF;
- resize handling failure;
- catchable termination signals routed through orderly cleanup.

No cleanup guarantee is possible for `SIGKILL`, process crash that prevents
cleanup, power loss or terminal disconnection. Do not install complex logic
inside a signal handler.

## 8. Acceptance tests

- split frame prefix/body over arbitrary read boundaries;
- partial writes followed by `AGAIN` then progress;
- `AGAIN` does not become EOF or spin;
- terminal and transport become readable together;
- ESC timeout fires while transport messages continue to be processed;
- Backend input is processed while the terminal is idle, and vice versa;
- SIGINT/SIGTERM set a shutdown request and cleanup runs in normal context;
- every tested exit path restores captured terminal/console state;
- no blocking protocol read can freeze the terminal event loop.


## 9. Incremental Core client (implemented)

`include/cdin/frontend/core/protocol/async_client.h` and
`core/src/protocol/async_client.c` add a readiness-safe API without changing
the existing Foundation client structure. It uses caller-owned, independent RX
and TX buffers; retains prefix/body and partial-write offsets; validates frame
length before accepting a body; checks Protocol version, direction and envelope
sequence; and advances the outbound sequence only after the complete frame is
written. `AGAIN` is a first-class status and never means EOF. The caller must
wait for the corresponding readiness notification before retrying.

`tests/core/integration/async_protocol_client.c` injects repeated `AGAIN`
results and two-byte progress to prove that partial reads/writes survive across
turns. This closes the Core partial-I/O primitive only. It does not supply a
POSIX/Windows wait-set, signal cleanup, transport endpoint or a live TUI pump.

Spec: [FE-019](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L82),
[Frontend Protocol](https://github.com/cdin-editor/spec/blob/main/docs/architecture/frontend-protocol.md).
