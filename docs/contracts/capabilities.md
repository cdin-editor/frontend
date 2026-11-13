# Frontend capability registry

**Status:** the registry and negotiation semantics are `ACCEPTED` by Spec
FE-003, FE-015, ADR-0018 and ADR-0020. This document records the Frontend-side
implementation contract. It does not claim that the current Backend 1.0.0
server or any adapter supports every registered capability.

## 1. Four separate concepts

Do not collapse these into one boolean:

1. **Registered:** the identifier and semantic meaning are known to the
   Protocol implementation.
2. **Requested:** the frontend declares that it can implement the capability
   in `hello`.
3. **Supported by Backend:** the Backend has a live implementation available
   in this process/session.
4. **Accepted:** the capability is present in `hello_ack.accepted_capabilities`
   for this specific connection.

Only accepted capabilities may be used. The effective set is the intersection
of the frontend's real implementation, Backend's real implementation, the
registered semantics and the current session's accepted list.

`units` (`cells` or `pixels`) is attach metadata, not a capability.

## 2. Registry

The following names are defined by the Spec. They are not a statement that the
current Frontend or Backend supports them all.

| Name | Meaning | TUI implementation target | GUI-SDL implementation target |
| --- | --- | --- | --- |
| `clipboard` | Clipboard get/set request and response | Optional; only where terminal/host integration can satisfy it | Optional; platform clipboard service |
| `dialogs` | Confirm/open/save dialog requests | Optional; terminal prompts are not automatically equivalent to native dialogs | Optional; native or GUI-owned dialogs |
| `canvas` | Free-form plugin drawing surface | Not in baseline; report unavailable | Optional, isolated plugin canvas surface |
| `mouse` | Pointer input events | Optional terminal mouse-reporting mode | Desktop pointer events |
| `hit_regions` | Environment-specific hit regions such as title-bar controls | Not applicable in baseline | Window decorations/custom title bar only if implemented |
| `rich_text` | Extended styled-text presentation beyond mandatory semantic spans | Graceful style degradation | Richer style mapping |
| `images` | Image-bearing UI content | Not in baseline unless terminal image protocol is explicitly added | Optional texture/image rendering |
| `truecolor` | Full 24-bit foreground/background color | Depends on terminal capability detection | Supported by renderer when enabled |
| `ime` | Input-method composition; committed result travels through `input.text` | Terminal/platform dependent | Desktop input-method integration |
| `byte-anchored-lines/1` | Version-pinned byte scroll anchor resolved to a logical line by Backend; `first_byte` identifies the returned window start | Not requested by the current TUI adapter | Opt-in GUI-SDL bounded line-window scrolling; requires same-snapshot Backend support |
| `ui-presentation/1` | ADR-0026 typed style-role, panel, prompt and declarative-widget payloads; interactive use requires explicit `HELLO_ACK` acceptance | Not requested; no complete widget renderer yet | Not requested; style-role decoding exists, but panel/widget rendering and shared vectors are not complete |

The Headless adapter can emulate any registered capability for tests, but its
reported accepted set must describe the fake implementation explicitly; it
must not pretend a real clipboard or dialog exists.

## 3. Negotiation rules

- Capability identifiers are unique UTF-8 text values.
- The frontend sends only capabilities that the selected adapter can actually
  service in the current process.
- `hello_ack.accepted_capabilities` is authoritative for the connection.
- Unknown capability names are ignored according to the Protocol contract;
  they are not converted into implicit support.
- A capability removed by Backend negotiation must be treated as unavailable
  immediately, including for outstanding UI features.
- A capability may be supported by an adapter but unavailable for one
  connection; support is not the same as acceptance.
- Capability state is cleared when the session disconnects and renegotiated
  on reconnect.
- Do not infer `mouse`, `clipboard`, `dialogs`, `canvas`, `images`, `ime` or `ui-presentation/1`
  from `units`, terminal type, or platform name.

## 4. Foundation 1.0.0 Core API

The stable UI-neutral registry is implemented in:

- `include/cdin/frontend/core/capabilities.h`
- `core/src/capabilities.c`
- `cdin_frontend_session_set_capabilities()` and the `hello_ack` path in
  `core/src/session/session.c`

The registry uses a fixed 64-bit mask with stable bit identities for the registered
Spec names. `cdin_frontend_capabilities_parse_names()` and
`cdin_frontend_capabilities_parse_cbor_array()` validate UTF-8 identifiers,
reject duplicates, ignore unknown identifiers, and return only registered
bits. `cdin_frontend_capabilities_validate_accepted()` rejects any known bit
that was not requested. The session defaults to an empty requested set; an
adapter must explicitly set the mask before sending `hello`.

The Core registry is metadata and negotiation state only. It does not create
or emulate platform services. Adapter code must derive the requested mask from
capabilities it can actually service. The accepted mask returned by the
Backend is connection-scoped and is the only mask that authorizes optional
Protocol behavior.

## 5. Required tests

- each registered identifier is unique and has a stable spelling;
- duplicate requested identifiers are rejected or canonicalized consistently
  with the Spec vector;
- unknown identifiers do not become active;
- requested-but-not-accepted capabilities remain unavailable;
- accepted capabilities cannot exceed the concrete adapter's implementation;
- capability state is reset on detach/reconnect;
- clipboard/dialog/canvas messages are rejected locally when the capability
  was not accepted;
- TUI and GUI announce different sets when their real implementations differ.

## 6. Current state

- **Foundation API:** stable Core capability registry and session negotiation
  helpers are part of Foundation 1.0.0.
- **Protocol:** the Frontend implementation is 1.0.0. Protocol 1.0.0
  capability/schema behavior is not claimed.
- **Backend:** the paired Backend 1.0.0 source advertises no optional
  capabilities, so the accepted set is currently empty in the paired source
  integration path.
- **Adapters:** the TUI preview defaults to an empty requested set. It must not
  advertise mouse, clipboard, dialogs, canvas, images, truecolor, IME or
  `ui-presentation/1` until
  each implementation and its tests exist. In particular, `ui-presentation/1`
  remains registered-but-not-requested until the GUI-SDL/TUI schema renderer,
  Backend validation and shared ADR-0026 vectors all pass. GUI-SDL supports
  opt-in IME, splitter
  and byte-anchored line windows in component builds; normal product builds keep
  their compile-time gates disabled until CDIN #118 live E2E passes. Headless
  records protocol messages but does not implement optional device services.

The registry itself is implemented; concrete adapter support and Protocol
1.0.0 interoperability are separate release gates. The byte-anchor implementation
is tracked by [Frontend #45](https://github.com/cdin-editor/frontend/issues/45)
and [Backend #30](https://github.com/cdin-editor/backend/issues/30), with product
acceptance in [CDIN #118](https://github.com/cdin-editor/cdin/issues/118).
