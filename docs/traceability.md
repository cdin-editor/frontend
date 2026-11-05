# Spec traceability

This table distinguishes Spec decisions from code status. `ACCEPTED` in the
Spec means the owner accepted the contract; it does not mean the source
implements it. Status below describes this source snapshot.

| Spec reference | Contract | Frontend source | Status |
| --- | --- | --- | --- |
| [FE-001](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L63) / [PROTO-050](https://github.com/cdin-editor/spec/blob/main/docs/architecture/protocol-wire.md) | All frontends are Protocol peers; no private Backend channel | `core/src/protocol/`, `core/src/session/` | `IMPLEMENTED` for the 1.0.0 wire path; async client is additive |
| [FE-002](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L64) / [ARCH-043](https://github.com/cdin-editor/spec/blob/main/docs/architecture/module-boundaries.md) | No Kernel, Runtime or Backend private dependency | `scripts/check-boundaries.sh` | `IMPLEMENTED` by current static check |
| [FE-003](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L65) | Capability declaration and mandatory core namespace | `core/src/capabilities.c`, `core/src/session/session.c`, `wire.c` | `IMPLEMENTED` for registered-name and requested/accepted masks on Protocol 1.0.0; Backend optional-capability support remains empty |
| [FE-004](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L66) / [PROTO-001](https://github.com/cdin-editor/spec/blob/main/docs/architecture/protocol-wire.md#L82) | Presentation state is disposable | `core/src/model/state.c`, `ui/tui/src/model/model.c` | `PARTIAL — expanded`; bounded snapshot struct/validator now checks 32 views, 16 panels, tree, options/styles and atomic replace; wire projector remains |
| [FE-005](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L67) / [PROTO-023](https://github.com/cdin-editor/spec/blob/main/docs/architecture/protocol-wire.md#L84) | Discard stale lines and re-request | `core/src/model/state.c`, `ui/tui/src/model/model.c`, `ui/tui/src/session/session.c` | `PARTIAL`; bounded multi-view semantic snapshot and atomic replacement added; exact line-request identity/correlation and out-of-order response scheduling remain pending |
| [FE-006](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L68) / [ADR-0006](https://github.com/cdin-editor/spec/blob/main/docs/adr/ADR-0006-visual-layout-ownership.md) | Adapter owns shaping and bidi | TUI renderer and planned TUI/GUI text modules | `PARTIAL`; complete shaping/bidi absent |
| [FE-007](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L69) | Headless reference records/replays messages | [`ui/headless/src/recorder.c`](../ui/headless/src/recorder.c), [`recorder.h`](../ui/headless/include/cdin/frontend/ui/headless/recorder.h), [`recorder.c` test](../tests/ui/headless/integration/recorder.c) | `PARTIAL`; bounded validated trace format and deterministic replay primitive implemented; not yet wired to every live peer message or shared conformance corpus |
| [FE-008](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L70) | Non-interactive CLI | [`docs/frontends/cli.md`](frontends/cli.md) | `DEFERRED` by Spec; intentionally not implemented in the TUI-first product scope |
| [FE-009 / FE-011](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L71-L74) | Full-screen interactive TUI is first interactive target | [`ui/tui/`](../ui/tui/), [`apps/tui/main.c`](../apps/tui/main.c), [`event_loop.c`](../ui/tui/src/session/event_loop.c), [`event_loop.c` test](../tests/ui/tui/integration/event_loop.c) | `PARTIAL`; runnable read-only preview and readiness coordinator exist; live transport/Runtime integration and full editor behavior remain |
| [FE-010](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L73) | One repository with Core, GUI-SDL, TUI and Headless peers | `core/`, `ui/gui-sdl/`, `ui/tui/`, `ui/headless/`, `CMakeLists.txt` | `PARTIAL`; TUI/Headless libraries and TUI/GUI preview executables build independently; production GUI and live peer integration remain |
| [FE-012](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L75) | GUI/TUI do not share a rendering pipeline | [`docs/architecture/module-boundaries.md`](architecture/module-boundaries.md), [`scripts/check-boundaries.sh`](../scripts/check-boundaries.sh) | `ALIGNED`; enforce as new source arrives |
| [FE-013](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L76) / [ADR-0011](https://github.com/cdin-editor/spec/blob/main/docs/adr/ADR-0011-extension-ui.md) | Declarative plugin widgets; optional canvas | [`docs/contracts/semantic-model.md`](contracts/semantic-model.md), TUI renderer | `PARTIAL — semantic foundations only`; bounded panel/style values exist, but declarative widget schema, parser, renderer and optional-canvas fallback remain |
| [FE-014](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L77) / [ADR-0019](https://github.com/cdin-editor/spec/blob/main/docs/adr/ADR-0019-runtime-bridge.md) | Lua 5.4 policy; native Frontend/Backend bridge boundaries | Frontend C only; no Runtime linkage; [`scripts/check-boundaries.sh`](../scripts/check-boundaries.sh) | `ALIGNED` at this repository boundary |
| [FE-015](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L78) | Central capability registry and accepted-set handling | `include/cdin/frontend/core/capabilities.h`, `core/src/capabilities.c`, `core/src/session/session.c` | `IMPLEMENTED` in Foundation Core 1.0.0; concrete adapter masks remain pending |
| [FE-016](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L79) / PROTO-041 | `input.key` distinct from committed `input.text`; command request remains a Protocol message | Core input/session, `core/src/session/command.c`, TUI parser | `PARTIAL`; typed `request.command` forwarding now has an installed-session API and a protocol regression test; Runtime owns command execution and broader text/IME adapter coverage remains |
| [FE-017](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L80) | Nullable active view and typed `ui.options.options` | current wire 1.0.0 | `IMPLEMENTED` for 1.0.0 target |
| [FE-018](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L81) | Adapter owns grapheme-aware text presentation | current TUI renderer | `PARTIAL`; cell/grapheme/bidi mapping incomplete |
| [FE-019](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L82) | Readiness-driven TUI loop and orderly restore | [`event_loop.h`](../ui/tui/include/cdin/frontend/ui/tui/event_loop.h), [`event_loop.c`](../ui/tui/src/session/event_loop.c), [`event_loop.c` test](../tests/ui/tui/integration/event_loop.c), [`async_client.c`](../core/src/protocol/async_client.c), [`async_protocol_client.c` test](../tests/core/integration/async_protocol_client.c), `ui/tui/src/platform/terminal_*.c` | `PARTIAL`; fair bounded coordinator plus incremental Core partial-I/O client and `AGAIN`/progress integration test exist; the TUI adapter is now installable/exported, but the native wait bridge, signal cleanup, worker wakeup wiring and PTY/console restoration tests remain |
| [FE-020](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L86) | All SDL use is in GUI-SDL | [`ui/gui-sdl/app/main.c`](../ui/gui-sdl/app/main.c), [`scripts/check-boundaries.sh`](../scripts/check-boundaries.sh) | `IMPLEMENTED — source boundary`; all current SDL usage is confined to GUI-SDL; production GUI behavior remains partial under FE-010/FE-023 |
| [FE-021](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L87) / [FE-022](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L88) | Legacy `cdin` SDL/renderer migration evidence | [`cdin-editor/cdin`](https://github.com/cdin-editor/cdin) legacy sources | `EXISTING` evidence only in Spec; not Frontend implementation |
| [FE-023](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L89) | SDL services exposed through capabilities | GUI-SDL preview only | `NOT IMPLEMENTED`; preview advertises no capabilities until the real adapter can service them |
| PROTO-010..013 | Handshake, sessions, capabilities and roles | `core/src/session/session.c` | `IMPLEMENTED`; Protocol 1.0.0 |
| PROTO-020..024 | Atomic flush, backpressure, pull lines and versions | Core model and TUI model/session | `PARTIAL`; one-view subset only |
| PROTO-030..033 | Transport-independent semantics and deterministic CBOR | `core/src/protocol/wire.c` | `IMPLEMENTED`; Protocol 1.0.0 Core tests pass |
| ADR-0018 / ADR-0020 | Wire/client/driver and Frontend design decisions | protocol/architecture docs | `ALIGNED` with Protocol 1.0.0 source; Runtime transport integration remains pending |

## Version matrix

| Contract | Accepted target | Current inspected implementation |
| --- | --- | --- |
| Frontend Foundation Core API | `1.0.0` | `1.0.0`, stable installed Core package |
| Spec | `1.2.0` | Current design authority |
| Frontend Protocol | `1.0.0` | Frontend codec `1.0.0` |
| Backend Protocol | `1.0.0` | Backend codec/server `1.0.0` |
| Runtime Host API | `1.0.0` | Paired Runtime/Backend source snapshots report `1.0.0`; Frontend does not link this API directly |
| Kernel ABI | `1.1.0` | Kernel package `1.2.1`, ABI `1.1.0` |

Foundation Core API and Frontend Protocol are independently versioned at 1.0.0. The matching Backend source snapshot also implements Protocol 1.0.0, and the paired Runtime archive reports Host API 1.0.0. Paired Codec tests cover the updated schema invariants; live Runtime/Kernel integration, concrete transport and product-level conformance remain integration gates.

## Traceability update rule

Every new implementation must point to the Spec requirement/ADR, name its
source and executable test, and use one of the status labels in
[docs/README.md](README.md). An accepted design without tests is not
`IMPLEMENTED`.
