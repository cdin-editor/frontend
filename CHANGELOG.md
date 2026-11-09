## Unreleased — Registry integration candidate 1.1.0

The CMake/package version is now `1.1.0` for the next immutable TUI and
GUI-SDL Registry source identities. This additive package candidate includes
the optional typed PaneTree splitter resize API; it is not a formal `v1.1.0`
tag or GitHub Release. The Frontend Protocol wire contract remains `1.0.0`.

### Added

- Add stable split-node identity, nested PaneTree geometry and divider hit-testing to the SDL-independent GUI-SDL presentation foundation.
- Add reversible UTF-8 display mapping primitives with source-byte mapping for non-ASCII text.
- Harden borrowed Session detach and fail-closed adapter lifecycle handling, including repeated detach and renderer-recreation failure coverage.
- Add optional `pane-splitter-resize/1` capability and typed `pane.splitter.resize` / `pane.splitter.result` messages in the independent `pane` namespace.
- Add Driver- and capability-gated Session request API, stable split/revision validation, pointer capture, pixel-only preview, one release commit, and cancellation on stale layout, Escape, focus loss, detach and Driver loss.
- Keep committed PaneTree state unchanged until the matching Backend result and atomic `ui.layout` / `ui.flush` snapshot commit; test the lifecycle with SDL dummy and sanitizer coverage.

### Release limitations

- GUI-SDL still uses SDL debug-text rendering and lacks full Unicode shaping, bidi, font fallback and semantic widget/panel rendering.
- SDL text preedit/IME lifecycle remains incomplete. Splitter drag/commit is implemented and component-tested, but the optional capability remains disabled by default until CDIN pins compatible immutable candidates and passes its live product matrix.
- Interactive TUI/GUI product acceptance and reconnect gates remain separate from this component candidate. The formal 1.1.0 component candidate remains separate from the full Frontend product release gate by production GUI and product integration requirements.

## [1.0.0] - 2026-11-04

### Foundation and shared Protocol 1.0.0

- Stabilize and install the UI-neutral Core C API as `cdin::frontend_core` 1.0.0.
- Keep Foundation Core API and Frontend Protocol as independent versions; both are 1.0.0 in this source snapshot.
- Add the central capability-name registry, duplicate/UTF-8 validation, unknown-name handling, and requested/accepted capability masks in session negotiation.
- Limit Core snapshot bookkeeping to the Spec semantic-model maximum of 32 views.
- Export/install Core only; keep the partial TUI as a development preview outside the stable package.
- Document public API ownership, source/ABI compatibility, blocking callback I/O, and explicit product-release exclusions.


### Release-gate implementation update (working tree; not a product release)

- Add a transport-agnostic TUI readiness coordinator. It blocks in the injected composition-root wait callback, drains terminal and Backend sources under per-source budgets, routes resize/termination, and requires a shutdown callback for cleanup. A fake-readiness integration test covers simultaneous readiness, bounded draining and teardown notification. **FE-019 remains partial** until the real POSIX/Windows wait bridge, async session/transport pump, signal cleanup and PTY/console tests exist. Core now has a separate incremental Protocol I/O client.
- Add the bounded `CDHR` v1 Headless message-trace recorder/replayer. It validates Protocol 1.0.0 payloads, message direction, sequence/version, trace bounds and truncation; replay supports direction filtering and callback failure. **FE-007 remains partial** until live peer wiring, scripted input integration and shared conformance vectors are complete.
- Enable the Headless primitive in the default development/test configuration and add boundary checks that keep it free of display, terminal and peer-adapter dependencies.
- Add `docs/FRONTEND-1.0.0-RELEASE-GATE.md` with per-FE status, source paths, exact Spec line anchors, rationale and product acceptance scenarios; reconcile stale Protocol/Runtime version statements across the documentation.
- At the initial coordinator checkpoint, GCC 14.2 Release, Clang 17 Release and GCC ASan+UBSan each passed 14/14 tests. Boundary checker and local Markdown-link check passed. Production logger installation, markdownlint, native macOS/Windows and live product E2E were not run in this environment.

### Added

- Upgrade the independent Frontend Protocol implementation to shared wire version 1.0.0; require the typed `ui.options.options` map and accept nullable PaneTree `active_view`.
- Add v1.0.0 wire validation coverage for typed options and nullable active views, synchronized with Backend fixtures.

- Separate UI-neutral Frontend Core from peer UI implementations. Move the interactive terminal frontend into `ui/tui/`, reserve `ui/gui-sdl/` for the native desktop UI and `ui/headless/` for recording/replay. Add a Neovim-like TUI screen architecture and dependency-boundary documentation.

- Implement a Frontend-owned deterministic-CBOR codec and Protocol 1.0.0 frame/client/session path without linking the Backend reference codec.
- Add atomic TUI snapshot/response staging, version-pinned line windows, stale-version rejection, invalidation recovery and byte-anchor scrolling.
- Add POSIX and Windows terminal lifecycle adapters, incremental escape/UTF-8 input parsing, raw key forwarding, bracketed paste and bounded byte-safe rendering.
- Render Runtime-owned byte selections in the TUI without taking selection ownership from Runtime.
- Add independent `[FRONTEND][AREA]` diagnostics through `m-mdy-m/log.c`.
- Add CMake install/export package, Pipeline tasks, PSX metadata, gix flow and CTest integration coverage.

### Changed

- Build TUI as a peer preview target `cdin::frontend_ui_tui`; it is not installed/exported by Foundation 1.0.0. Core contains no terminal or SDL APIs.
- Remove the Backend `cdin_protocol` target dependency; Frontend implements the normative wire contract locally as allowed by ADR-0018.
- Pin `m-mdy-m/log.c` v0.1.0 as the only external library dependency. Kernel logger and Kernel package are not used.
- Distinguish the accepted interactive TUI target (FE-009/FE-011) from the deferred non-interactive CLI (FE-008). The current TUI implementation remains partial.

### Known limitations

- The accepted TUI target is not yet a complete runnable editor application. A readiness coordinator exists, but the `cdin` composition root must supply the real wait/transport bridge and integrated Runtime editing path.
- Multi-view panes, style-role rendering, complete Unicode grapheme/Bidi/Persian shaping, mouse support, and live Headless reference-peer wiring/shared conformance remain incomplete. The bounded Headless trace primitive exists.
- The SDL desktop GUI has not been implemented.
- Native verification against the upstream logger archive and the full Linux/macOS/Windows CI matrix remain release gates.

### Frontend design alignment (2026-11-04)

- Expand architecture, module-boundary, Protocol migration, capability, semantic-model, text-presentation and event-loop documentation against CDIN Spec 1.2.0.
- Document TUI as the accepted first interactive target while retaining its partial implementation status.
- Add detailed Headless and GUI-SDL designs, a Protocol 1.0.0 upgrade plan, verification gates and requirement traceability.
- Remove the obsolete local TUI proposal as an authority; link to the accepted Spec ADR-0020.


## Unreleased — semantic snapshot foundation

- Added a bounded, UI-neutral `cdin.ui-model.v1` semantic snapshot value API.
- Added allocation-free validation for view, pane-tree, panel, option and style
  bounds and references.
- Added atomic monotonic snapshot replacement: invalid or stale snapshots do
  not modify the committed model.
- Added regression tests for null layout, active-view membership, document
  selection bounds, duplicate/invalid references, tree cycles, typed option
  bounds, model limits and failed-commit atomicity.
- Added bounded UTF-8 validation and non-regressing per-view/document revision checks.
- Kept Protocol-to-snapshot projection, multi-view concurrent line requests, declarative
  widgets, concrete TUI/GUI applications and `cdin` transport integration gated.

## Unreleased — readiness I/O and line response correlation

- Added the ABI-additive async Protocol client with separate caller-owned RX/TX buffers, retained partial-frame offsets, and explicit `PROGRESS`/`AGAIN`/EOF/failure results.
- Added integration coverage for repeated `AGAIN`, partial two-byte reads/writes, truncated frame prefixes and oversized lengths.
- Added exact single-active-view TUI line request correlation by view, document version, first line and requested count; stale responses are ignored and unrequested/future responses are rejected.
- Added regression coverage for UTF-8 semantic IDs, monotonic per-view/document versions and stale/future line responses.
- Follow-up GCC, Clang and ASan+UBSan CTest runs each pass 16/16; Core install/export consumer smoke test and dependency-boundary check pass. Production logger installation and native platform E2E remain unverified.


## Unreleased — runnable adapter previews and developer targets

- Added a read-only TUI preview executable that exercises terminal open/render/input/restore; Runtime-owned editing and live Protocol transport remain external.
- Added an isolated SDL3 3.2+ desktop smoke-preview executable; it uses the Frontend logger and does not advertise capabilities or claim live editor behavior.
- Reworked Makefile targets: `make build`, `make build-tui`, `make run-tui`, `make build-gui`, `make run-gui`, `make test`, and `make help`.
- Added optional GUI-SDL CMake discovery and documented SDL3/logger prerequisites.
- Updated Spec traceability, status and build guides to distinguish runnable previews from complete product adapters.
