# FE-ADR-0001: readiness-driven client I/O and event-loop boundary

- **Status:** `PROPOSED`; implementation API and callback table are not frozen.
- **Scope:** Frontend-local implementation decision.
- **Related:** Spec FE-019, PROTO-030..033, ADR-0018, ADR-0020.

## Context

The current Frontend Core I/O table exposes blocking `read`/`write`
callbacks. The TUI terminal adapter also exposes blocking read/write and size,
but has no readiness wait. A single interactive loop must service terminal
input, Backend protocol frames, resize and termination without blocking one
source or busy-waiting.

## Problem

How does Frontend Core represent partial progress, temporary unavailability,
EOF and fatal I/O, while allowing each adapter to wait for the right set of
platform events without leaking OS or toolkit types into Core?

## Proposed direction

1. Replace the whole-frame blocking assumption with incremental bounded
   Protocol read/write state.
2. Distinguish `PROGRESS`, `AGAIN`, `EOF` and `ERROR` unambiguously.
3. Add a wait/readiness bridge supplied by the composition root. Core does not
   own sockets, file descriptors, SDL events, `HANDLE`s or `pollfd`s.
4. Let TUI combine terminal and transport readiness; let GUI combine SDL and
   transport readiness; let Headless inject deterministic readiness.
5. Keep all callbacks and waits outside Kernel and Runtime APIs. Runtime/Lua
   remains on the Backend owner thread.
6. Add explicit struct-size/version evolution to any public callback table.
7. Bound pending input/output frames and guarantee progress or a reported
   no-progress state; never turn `AGAIN` into EOF.

## Alternatives

- **Polling/sleep loop:** rejected; wastes CPU, adds latency and complicates ESC
  and signal handling.
- **Blocking whole-frame read:** rejected for interactive adapters; can starve
  terminal/window input.
- **OS handles in Core:** rejected; breaks Core's platform-neutral contract.
- **One event loop in Backend for all UIs:** incompatible with independent
  peer frontends; Backend continues to own only its own Runtime pump/session.

## Consequences

The exact callback function names, wait-handle representation and ownership
lifetime remain open until the composition-root transport API is reviewed.
The current Core client must not be described as readiness-capable before the
new interface and its fault tests exist.

## Acceptance tests

- partial frame read/write and resumption;
- AGAIN distinct from EOF;
- simultaneous terminal/transport readiness;
- bounded work without starvation;
- ESC timeout without sleeps/busy-wait;
- cleanup on error/termination;
- no OS type in Core public headers.
