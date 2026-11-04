# Frontend 1.0.0 product release gate

**Authority:** CDIN Spec 1.2.0. This document is the release decision record for
the complete Frontend product, not just the already-versioned Foundation Core
package. Requirement status describes this source tree and must be updated only
after the named tests pass.

## Release decision

**Product release: NOT READY.** Foundation Core API 1.0.0 and Frontend Protocol
1.0.0 are independently versioned contracts. They do not imply that TUI,
Headless, GUI-SDL or the complete CDIN application are shippable.

The non-interactive CLI is deliberately excluded: Spec FE-008 remains
`DEFERRED`, and FE-009/FE-011 explicitly select the full-screen TUI as the first
interactive target. Implementing a CLI now would create a second user-facing
command policy surface before the required TUI and reference adapter exist.

## Requirement-by-requirement gate

| Spec | Current implementation and evidence | Status | Remaining work and rationale |
|---|---|---|---|
| [FE-001](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L63) | [`core/src/protocol/client.c`](../core/src/protocol/client.c), [`core/src/protocol/wire.c`](../core/src/protocol/wire.c), [`docs/contracts/frontend-protocol.md`](contracts/frontend-protocol.md) | `IMPLEMENTED` for Protocol 1.0.0 Core | Keep the peer-only boundary; live transport conformance is a product integration gate. |
| [FE-002](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L64) | [`scripts/check-boundaries.sh`](../scripts/check-boundaries.sh) | `IMPLEMENTED` by static boundary check | Keep this check mandatory in CI; it prevents accidental coupling to Kernel, Runtime or Backend internals. |
| [FE-003](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L65) | [`core/src/capabilities.c`](../core/src/capabilities.c), [`core/src/session/session.c`](../core/src/session/session.c) | `PARTIAL` | The registry and requested/accepted set validation exist. Each adapter still needs to report only services it can actually perform; otherwise a peer could invoke a capability that is merely named, not implemented. |
| [FE-004](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L66) | [`include/cdin/frontend/core/model/semantic.h`](../include/cdin/frontend/core/model/semantic.h), [`core/src/model/semantic.c`](../core/src/model/semantic.c), [`tests/core/unit/semantic_model.c`](../tests/core/unit/semantic_model.c) | `PARTIAL — expanded` | A bounded semantic snapshot value/store now validates views, layout, options, panels and styles and commits atomically. Protocol-to-model projection and adapter-cache invalidation are still required; a C value type alone does not complete the inbound UI pipeline. |
| [FE-005](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L67) | [`core/src/model/state.c`](../core/src/model/state.c), [`core/src/model/semantic.c`](../core/src/model/semantic.c), [`tests/core/unit/semantic_model.c`](../tests/core/unit/semantic_model.c) | `PARTIAL — expanded` | Snapshot retains bounded multi-view metadata, validates bounded UTF-8 values and rejects stale snapshot/view/document revisions atomically. TUI exact single-active-view request version/range correlation and stale/future response tests are implemented; multi-view concurrent requests and protocol-to-snapshot projection remain. |
| [FE-006](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L68) | [`ui/tui/src/render/tui.c`](../ui/tui/src/render/tui.c), [`docs/contracts/text-presentation.md`](contracts/text-presentation.md) | `PARTIAL` | Complete shaping and bidi are absent. This is essential for Persian/Arabic and mixed-direction text; ASCII-only rendering is not a safe substitute. |
| [FE-007](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L69) | [`recorder.c`](https://github.com/cdin-editor/frontend/blob/main/ui/headless/src/recorder.c#L54-L202), [`recorder.h`](https://github.com/cdin-editor/frontend/blob/main/ui/headless/include/cdin/frontend/ui/headless/recorder.h#L1-L67), [`recorder test`](https://github.com/cdin-editor/frontend/blob/main/tests/ui/headless/integration/recorder.c#L1-L109) | `PARTIAL` | The bounded validated `CDHR` trace primitive is implemented. Wire it to every live receive, add scripted input through the ordinary client/session path, and run shared positive/negative vectors. The reference peer must not bypass the production Protocol implementation. |
| [FE-008](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L70) | [`docs/frontends/cli.md`](frontends/cli.md) | `DEFERRED` by Spec | Do not implement or advertise CLI as part of this release. It is a non-interactive command runner, distinct from the TUI; implementing it now would contradict the accepted delivery order. |
| [FE-009 / FE-011](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L71-L74) | [`ui/tui/`](../ui/tui/), [`apps/tui/main.c`](../apps/tui/main.c), [`event_loop.c`](https://github.com/cdin-editor/frontend/blob/main/ui/tui/src/session/event_loop.c#L37-L100) | `PARTIAL` | A runnable read-only TUI preview now exercises terminal open/render/input/restore. It is not a connected editor: live Backend pump, full PaneTree, semantic projection and Runtime editing flow remain. |
| [FE-010](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L73) | [`core/`](../core/), [`ui/tui/`](../ui/tui/), [`ui/headless/`](../ui/headless/), [`ui/gui-sdl/`](../ui/gui-sdl/) | `PARTIAL` | Core/TUI/Headless build targets and runnable TUI preview exist; GUI-SDL now has an isolated SDL3 smoke-preview executable. Production peer behavior, tests and live transport remain. |
| [FE-012](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L75) | [`docs/architecture/module-boundaries.md`](architecture/module-boundaries.md), [`scripts/check-boundaries.sh`](../scripts/check-boundaries.sh) | `ALIGNED` | Preserve independent GUI pixel/font layout and TUI cell layout. Do not extract a shared renderer merely to reduce code duplication; only non-presentation contracts should be shared. |
| [FE-013](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L76) | [`docs/contracts/semantic-model.md`](contracts/semantic-model.md), current renderer | `PARTIAL — semantic foundations only` | The semantic snapshot now retains bounded panel IDs/kinds and semantic style roles. Declarative widget schema, widget tree parsing/rendering, bounded widget state and graceful fallback when optional `canvas` is absent are still required. Without this, plugin UI cannot be represented consistently by the accepted semantic model. |
| [FE-014](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L77) | Frontend C-only boundary; [`scripts/check-boundaries.sh`](../scripts/check-boundaries.sh) | `ALIGNED` at repository boundary | Keep Runtime Lua policy in Runtime and keep Frontend native code free of Lua/Kernel private interfaces. Verify the actual composition root in `cdin` before product release. |
| [FE-015](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L78) | [`include/cdin/frontend/core/capabilities.h`](../include/cdin/frontend/core/capabilities.h), [`core/src/capabilities.c`](../core/src/capabilities.c) | `PARTIAL` | Registry names and subset validation exist. Concrete adapters must provide supported masks per session and must never activate a capability solely because it was requested. |
| [FE-016](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L79) | [`include/cdin/frontend/core/input/event.h`](../include/cdin/frontend/core/input/event.h), [`ui/tui/src/input/parser.c`](../ui/tui/src/input/parser.c) | `PARTIAL` | Physical key identity and committed text are separated in the contract/parser. Complete IME, paste, terminal key coverage and observer/driver integration remain. Runtime must remain the only keymap interpreter. |
| [FE-017](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L80) | [`core/src/protocol/wire.c`](../core/src/protocol/wire.c), [`tests/core/unit/protocol_wire.c`](../tests/core/unit/protocol_wire.c) | `IMPLEMENTED` at Protocol codec level | Keep the typed option and nullable active-view fixtures shared with Backend. A complete semantic model must still retain the validated values after decoding. |
| [FE-018](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L81) | [`ui/tui/src/render/tui.c`](../ui/tui/src/render/tui.c), [`docs/contracts/text-presentation.md`](contracts/text-presentation.md) | `PARTIAL` | Add byte-to-code-point-to-grapheme-to-cell maps, combining/wide/emoji behavior, visual-line movement, wrapping and bidi. Logical positions must remain byte offsets; otherwise edits can corrupt multibyte text. |
| [FE-019](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L82) | [`ui/tui/include/cdin/frontend/ui/tui/event_loop.h`](../ui/tui/include/cdin/frontend/ui/tui/event_loop.h), [`async_client.h`](../include/cdin/frontend/core/protocol/async_client.h), [`async_client.c`](../core/src/protocol/async_client.c), [`event-loop test`](https://github.com/cdin-editor/frontend/blob/main/tests/ui/tui/integration/event_loop.c#L1-L116) | `PARTIAL` | The coordinator blocks in an injected readiness wait and drains both ready sources under bounded budgets. Core now supplies additive nonblocking partial-frame I/O with `AGAIN`/EOF/error distinction. `cdin` must still supply and test the real wait bridge and async session/transport pump; TUI still needs signal-to-cleanup routing and PTY/console restoration tests. |
| [FE-020](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L86) | [`ui/gui-sdl/app/main.c`](../ui/gui-sdl/app/main.c) | `PARTIAL` | SDL calls are confined to the GUI-SDL subtree and its target. The ownership boundary is in place for the preview; the complete GUI adapter remains unfinished. |
| [FE-021 / FE-022](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L87-L88) | Spec's historical `cdin` migration anchors | `EXISTING` evidence only | These are not standalone Frontend deliverables and must not be counted as completed GUI-SDL work. |
| [FE-023](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L89) | [`core/src/capabilities.c`](../core/src/capabilities.c), planned GUI adapter | `NOT IMPLEMENTED` as GUI behavior | Add `hit_regions` and `dialogs` only when SDL GUI implements and tests them. Advertising them earlier would violate capability negotiation semantics. |

## Additional release gates outside the Frontend repository

| Gate | Repository evidence | Why it blocks product 1.0.0 |
|---|---|---|
| Concrete transport and process lifecycle | [`cdin-editor/cdin`](https://github.com/cdin-editor/cdin) | The Frontend deliberately does not open sockets or own Backend processes. The composition root must provide a tested transport and readiness bridge. |
| Backend Protocol peer | [`cdin-editor/backend` Protocol implementation](https://github.com/cdin-editor/backend/tree/main/src/protocol), [Backend integration guide](https://github.com/cdin-editor/backend/blob/main/docs/frontend-integration.md) | The Backend must negotiate the same 1.0.0 wire schema and preserve flush, sequence, role and capability semantics. |
| Runtime UI policy and event dispatch | [`cdin-editor/runtime` Host API contract](https://github.com/cdin-editor/runtime/blob/main/docs/contracts/runtime-host-api.md) | Runtime owns semantic state and keymaps. The Frontend cannot compensate for missing Runtime model projection or event dispatch. |
| Kernel data integrity | [`cdin-editor/kernel` ABI](https://github.com/cdin-editor/kernel/blob/main/docs/abi.md) | End-to-end editing must preserve original bytes, line endings and document versions through Backend/Runtime/Kernel. |

## Required product acceptance scenarios

1. Launch the TUI through `cdin` against the real Backend/Runtime/Kernel stack.
2. Open an empty document and a document containing NUL bytes, CRLF, invalid UTF-8 bytes, combining marks, wide characters, emoji and Persian/Arabic mixed-direction text.
3. Type, paste, move, select, delete, undo, redo and save; compare saved bytes exactly with the expected result.
4. Resize repeatedly while both terminal input and Backend updates are active; prove neither source starves and the loop does not busy-wait.
5. Disconnect the Backend during partial reads/writes; prove deterministic error handling and terminal restoration.
6. Send malformed frames, stale line responses, duplicate sequence numbers, invalid flush groups and unrequested capabilities; prove fail-closed behavior and no false ACK.
7. Run Headless recording/replay over the same scenarios and compare deterministic traces.
8. Build and run native Linux, macOS and Windows CI; run ASan/UBSan, supported TSan and fuzz targets.
9. Build and install the production package using the real `m-mdy-m/log.c` dependency, then compile an external consumer against the installed targets.

## Release rule

Do not tag Frontend Product 1.0.0 while any accepted FE requirement above is
`PARTIAL`, `NOT IMPLEMENTED` or `BLOCKED`, or while any external integration
gate is unverified. Keep Foundation Core API 1.0.0 and Protocol 1.0.0 version
labels independent from the product release decision.
