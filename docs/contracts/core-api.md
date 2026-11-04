# Frontend Foundation Core API 1.0.0

**Status:** `STABLE` for the UI-neutral C Core API shipped as
`cdin::frontend_core`. This document does not declare the Frontend Protocol,
TUI, GUI-SDL, Headless adapter, or complete CDIN application to be 1.0.0.

## 1. Version identity

These version labels are independent:

| Contract | Version | Meaning |
|---|---:|---|
| Frontend Foundation Core API | `1.0.0` | Stable public C API and installed CMake target in this release. |
| Frontend Protocol implementation | `1.0.0` | Exact wire version implemented by `wire.c`, `client.c` and `session.c`. |
| Accepted Frontend Protocol target | `1.0.0` | Implemented by the Frontend codec/client source; cross-repository live conformance remains a separate gate. |
| CDIN Spec | `1.2.0` | Architecture and contract authority. |
| Runtime Host API | `1.0.0` | Paired Runtime/Backend source snapshots; Frontend does not call it directly. |
| Kernel C ABI | `1.1.0` | Backend-only Kernel boundary; Frontend does not include it. |

The Foundation package version MUST NOT be used as the negotiated Protocol
version. `version.h` exposes the Foundation version and records the Protocol
version supported by this source package. `wire.h` remains authoritative for
Protocol codec constants.

## 2. Stable package surface

The installed package provides the static CMake target:

```cmake
find_package(cdinFrontend 1.0 CONFIG REQUIRED)
target_link_libraries(my_frontend PRIVATE cdin::frontend_core)
```

The stable public headers are all headers under
`include/cdin/frontend/core/`:

| Header | Stable responsibility |
|---|---|
| `version.h` | Independent Foundation and Protocol version constants. |
| `capabilities.h` | Registered capability identifiers, masks and negotiation validation. |
| `diagnostics/log.h` | Frontend-owned logger facade. |
| `input/event.h` | Physical key, committed-text and paste event value type. |
| `protocol/wire.h` | Protocol 1.0.0 deterministic-CBOR envelope, payload and frame functions. |
| `protocol/client.h` | Caller-owned client state and framed send/receive operations. |
| `session/session.h` | Handshake, attach, driver, input, line request, ACK and detach state machine. |
| `model/state.h` | Snapshot/response flush bookkeeping and per-view document-version tracking. |

Only these Core headers and the Core library are installed/exported by the
Foundation 1.0.0 package. `ui/tui/include/` and
`ui/headless/include/` are intentionally not installed. TUI and Headless are
source/build preview targets and their APIs are not covered by the Foundation
1.0.0 stability guarantee. GUI-SDL remains unimplemented.

## 3. Compatibility policy

- The public language is C11. C++ consumers may include the C headers through
  their `extern "C"` guards; this does not promise a C++ object ABI.
- Within Foundation major version 1, existing function signatures, enum
  numeric values, macro meanings and public structure layouts MUST NOT change.
- Minor releases may add new functions, constants, enum values where unknown
  values are already documented as rejectable, and new opaque types. They MUST
  NOT append fields to an existing public structure.
- Any change to an existing public structure layout, function signature,
  existing enum value or ownership rule requires Foundation major 2.
- Bug fixes may tighten validation only where the 1.0.0 contract already
  requires rejection. They must not reinterpret previously valid inputs.
- The package is currently built as a static library. Consumers MUST link the
  library and headers from a consistent package release; this release does not
  define a separately loaded plugin ABI.
- Protocol evolution follows the independent Spec Protocol version rules. A
  Foundation 1.x release may support a newer Protocol minor or major only when
  its `wire.h`, client/session code and conformance evidence say so.

## 4. Ownership and lifetime

- `cdin_frontend_client`, `cdin_frontend_session` and
  `cdin_frontend_model_state` are caller-allocated value objects. Core does not
  allocate or free those objects.
- The caller owns the `frame_buffer` passed to
  `cdin_frontend_client_init`; it remains writable and alive until the client
  is no longer used. Its capacity MUST be at least
  `max_frame_bytes + CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES`.
- The `io.context` and callback code supplied to the client MUST remain valid
  for the complete client lifetime.
- The optional logger is borrowed. It MUST outlive every Core object that
  references it and may be destroyed only after those objects are no longer
  used.
- A message payload returned by `cdin_frontend_client_receive` borrows the
  client's frame buffer and is valid only until the next receive on that
  client, or until the buffer is reused/destroyed.
- A payload passed to `cdin_frontend_client_send` remains caller-owned. The
  send operation does not retain it after returning.
- Core creates no worker threads and has no process-global session state.

## 5. Threading and I/O

Core value objects are not internally synchronized. The caller MUST serialize
all access to one client, session or model-state object. Distinct objects may
be used on distinct threads subject to the logger dependency's own contract.
No callback is invoked by Core except the supplied transport callbacks during
explicit send/receive calls.

The 1.0.0 client uses the blocking callback contract in `wire.h`:

- A positive callback result reports the number of bytes transferred; it may
  be smaller than requested and Core continues until the complete frame has
  transferred.
- A zero read result is EOF. A negative result is an I/O failure.
- `AGAIN`/would-block is not a distinct result in this API and MUST NOT be
  represented as EOF. A nonblocking transport cannot be passed directly to
  `client_receive`/`client_send` unless the composition root adapts it to the
  blocking callback contract on a worker or supplies complete frames through
  lower-level encode/decode calls.
- If a write fails after making partial progress, the stream may contain a
  partial frame. The caller MUST close/discard that transport; it MUST NOT
  retry the same message on the same stream as though no bytes were written.
- `client_receive` may block while waiting for the remainder of a frame. It
  MUST NOT be called on a UI event thread that must continue processing device
  input or window events.

A readiness-driven `AGAIN`/wait API remains a future Core revision. This is an
explicit limitation of Foundation 1.0.0, not a hidden guarantee.

## 6. Protocol and session behavior

The Core wire codec implements exactly Frontend Protocol `1.0.0`:

- four-byte unsigned big-endian frame-body length;
- deterministic CBOR envelope and `core` namespace;
- 1 MiB default maximum body length;
- byte strings for document/paste data;
- strict frame/schema validation, message direction and per-connection
  sequence checks;
- one Driver and zero or more Observers as defined by the Protocol contract;
- atomic snapshot/response groups committed at `ui.flush` and acknowledged by
  flush-group sequence, never envelope sequence.

The client negotiates exact Protocol major/minor `1.0` and rejects a peer
that offers an unsupported version. The wire codec validates nullable
`active_view` and typed `ui.options.options`. Full semantic-model retention,
shared exhaustive conformance vectors and live Runtime/transport integration
remain separate product gates.

## 7. Capability registry contract

The registry defines the Spec names `clipboard`, `dialogs`, `canvas`, `mouse`,
`hit_regions`, `rich_text`, `images`, `truecolor` and `ime`.

- `requested_capabilities` are the capabilities the selected adapter actually
  implements and declares in `hello`; they are not a wish list.
- `accepted_capabilities` are copied from `hello_ack` and are scoped to that
  connection. They are authoritative for use in that session.
- Unknown UTF-8 capability identifiers are ignored and never become active.
  Duplicate identifiers and malformed names are rejected.
- The accepted known mask MUST be a subset of the requested known mask.
- Registry name lookup returns immutable strings with process lifetime. The
  parser accepts at most 4096 identifiers per negotiation; each known protocol
  identifier is limited to 63 UTF-8 bytes by this registry.
- The Core registry does not initialize clipboard, dialogs, canvas, mouse or
  IME services. Adapters are responsible for supplying the real supported mask.
- A newly initialized session requests no optional capabilities by default.
  Adapters must call `cdin_frontend_session_set_capabilities` before `hello`
  to advertise their implemented set.

The paired Backend 1.0.0 server advertises no optional capabilities. The TUI
preview and Headless recorder do not set a non-empty capability mask.

## 8. Semantic model boundary

`model/state.h` is intentionally bookkeeping rather than a complete Runtime
UI model. It records the committed model revision, flush-group sequence and
up to 32 view IDs with their document versions/invalidation state. It does not
own layout trees, panels, styles, selections, commands, keymaps or canonical
text. It does not replace the full `cdin.ui-model.v1` validator/projector
specified by CDIN Spec.

## 9. Not included in Foundation 1.0.0

The following are not shipped or guaranteed by this package:

- complete semantic UI model and atomic multi-view/panel application;
- readiness-driven partial-progress Protocol I/O and a concrete platform wait bridge;
- socket, named-pipe, authentication or encryption implementation;
- complete Headless reference peer and shared conformance runner (the bounded trace primitive exists);
- a runnable interactive TUI application or complete multi-pane UI;
- GUI-SDL, its renderer or its font/shaping stack;
- complete grapheme, bidi, Persian/Arabic presentation and hit-testing;
- live composition-root E2E against Backend, Runtime and Kernel;
- a non-interactive CLI frontend.

These are separate adapter, transport or cross-repository milestones. Their
absence does not make the stable Core API provisional; it does prevent the
repository from claiming that a complete CDIN Frontend product is ready.
