# Changelog

All notable changes to CDIN Frontend are documented in this file.

The format follows [Keep a Changelog].
Versioning follows [Semantic Versioning].

Frontend package versions and the Frontend Protocol wire-contract version are
independent. The entries below describe the component package; they do not by
themselves declare the complete TUI, GUI-SDL, or CDIN application production
ready. Optional IME and pane-splitter capabilities remain gated on compatible
immutable component candidates and live CDIN product verification.

## [1.1.1] - 2026-11-11

### Added

- Prepare immutable Core, TUI and GUI-SDL CMake-prefix package releases for
  Linux x86_64/ARM64, macOS x86_64/ARM64 and Windows x86_64 UCRT64.
- Generate three canonical ADR-0028 Component Release Metadata assets
  (`frontend-core-1.1.1.json`, `frontend-tui-1.1.1.json` and
  `frontend-gui-sdl-1.1.1.json`) from the exact tagged source commit after
  all 15 target/variant archives and their descriptors have been collected.

### Changed

- Keep the contributor source archive separate from installable variant
  packages. Core is published once per target and shared by TUI and GUI-SDL;
  SDL3 remains a separately provisioned environment package.
- Preserve the published 1.1.0 source-only Release and all earlier metadata
  identities unchanged. Package publication does not claim full TUI/GUI-SDL
  product readiness or enable optional capabilities by default.

## [1.1.0] - 2026-11-10

### Added

- Add stable split-node identity, nested PaneTree geometry, and divider
  hit-testing to the SDL-independent GUI-SDL presentation foundation.
- Add reversible UTF-8 display mapping primitives with source-byte mapping
  for non-ASCII text.
- Add the optional `pane-splitter-resize/1` capability and typed
  `pane.splitter.resize` / `pane.splitter.result` messages in the independent
  `pane` namespace.
- Add a Driver- and capability-gated Session request API with stable
  split/revision validation, pointer capture, pixel-only preview, one
  release-time commit, and cancellation on stale layout, Escape, focus loss,
  detach, or Driver loss.
- Keep committed PaneTree state unchanged until the matching Backend result
  and atomic `ui.layout` / `ui.flush` snapshot commit.
- Add the accepted `ime-composition/1` capability, typed
  `ime.composition.start/update/cancel` frames, and Core Session methods for
  byte-accurate UTF-8 preedit and composition-tagged commits.
- Capture SDL3 `SDL_EVENT_TEXT_EDITING` locally, convert SDL codepoint offsets
  to Protocol UTF-8 byte offsets, and render a local-only preedit, caret, and
  selection overlay without publishing preedit in document or snapshot state.
- Cancel local composition on Escape, native focus loss, view/version change,
  detach, and matching Backend cancellation. Preserve committed-only
  `input.text` fallback for peers that do not negotiate the capability.
- Add a bounded, UI-neutral `cdin.ui-model.v1` semantic snapshot value API
  with allocation-free validation for view, pane-tree, panel, option, style,
  UTF-8, and revision bounds.
- Add atomic monotonic snapshot replacement so invalid or stale snapshots do
  not modify the committed model.
- Add regression coverage for null layout, active-view membership, document
  selection bounds, duplicate/invalid references, tree cycles, typed option
  bounds, model limits, and failed-commit atomicity.
- Add an ABI-additive asynchronous Protocol client with separate caller-owned
  RX/TX buffers, retained partial-frame offsets, and explicit
  `PROGRESS`/`AGAIN`/EOF/failure results.
- Add exact single-active-view TUI line-request correlation by view,
  document version, first line, and requested count. Ignore stale responses
  and reject unrequested or future responses.
- Add a transport-agnostic TUI readiness coordinator with bounded per-source
  drains, resize/termination routing, and an explicit shutdown callback.
- Add the Frontend 1.0.0 release-gate document with per-requirement status,
  source paths, Spec anchors, and product acceptance scenarios.
- Add the bounded `CDHR` v1 Headless message-trace recorder/replayer with
  Protocol payload, direction, sequence/version, trace-bound, and truncation
  validation.
- Add a read-only TUI preview executable and an isolated SDL3 3.2+ desktop
  smoke-preview executable. Neither preview claims live Runtime editing or
  live Protocol transport.
- Add optional GUI-SDL CMake discovery and document SDL3/logger prerequisites.
- Add variant consumer configurations for Core, TUI, GUI-SDL, and combined
  package installation/export.

### Changed

- Keep the Frontend Protocol wire contract at `1.0.0`, independently of the
  Frontend Foundation package version.
- Keep transient IME preedit local to the GUI-SDL adapter; only committed text
  enters the Backend document path.
- Rework the Make targets for Core, TUI, GUI-SDL, previews, tests, and help.
- Update Spec traceability, status, and build documentation to distinguish
  reusable adapters and runnable previews from complete product adapters.

### Fixed

- Harden borrowed Session detach and fail-closed adapter lifecycle handling,
  including repeated detach and renderer-recreation failure coverage.
- Suppress adjacent duplicate SDL commit events only when both payload and
  native event timestamp match the already admitted composition commit;
  ordinary subsequent text remains on the committed-only path.
- Preserve atomic PaneTree state when a splitter request is rejected or
  cancelled, and reject stale split/revision results without mutating the
  committed model.
- Make the TUI line-response path ignore stale responses and reject
  unrequested or future responses.

## [1.0.0] - 2026-11-04

The 1.0.0 release stabilizes and installs the UI-neutral Core C API and the
shared Frontend Protocol 1.0.0 implementation. It is a component foundation
release, not a claim that the complete interactive editor application is
ready.

### Added

- Stabilize and install the UI-neutral Core C API as
  `cdin::frontend_core` 1.0.0.
- Add the central capability-name registry, duplicate/UTF-8 validation,
  unknown-name handling, and requested/accepted capability masks in session
  negotiation.
- Limit Core snapshot bookkeeping to the Spec semantic-model maximum of
  32 views.
- Upgrade the independent Frontend Protocol implementation to shared wire
  version 1.0.0; require the typed `ui.options.options` map and accept nullable
  PaneTree `active_view`.
- Add v1.0.0 wire validation coverage for typed options and nullable active
  views, synchronized with Backend fixtures.
- Separate UI-neutral Frontend Core from peer UI implementations. Move the
  terminal frontend into `ui/tui/`, reserve `ui/gui-sdl/` for the native
  desktop UI, and `ui/headless/` for recording/replay.
- Implement a Frontend-owned deterministic-CBOR codec and Protocol 1.0.0
  frame/client/session path without linking the Backend reference codec.
- Add atomic TUI snapshot/response staging, version-pinned line windows,
  stale-version rejection, invalidation recovery, and byte-anchor scrolling.
- Add POSIX and Windows terminal lifecycle adapters, incremental
  escape/UTF-8 input parsing, raw key forwarding, bracketed paste, and
  bounded byte-safe rendering.
- Render Runtime-owned byte selections in the TUI without taking selection
  ownership from Runtime.
- Add independent `[FRONTEND][AREA]` diagnostics through
  `m-mdy-m/log.c`.
- Add the CMake install/export package, Pipeline tasks, PSX metadata, gix
  flow, and CTest integration coverage.
- Add the Frontend 1.0.0 release-gate document with per-requirement status,
  source paths, Spec anchors, and product acceptance scenarios.
- Add detailed Headless and GUI-SDL designs, a Protocol 1.0.0 upgrade plan,
  verification gates, and requirement traceability.

### Changed

- Export/install Core as the stable Foundation package while keeping the
  partial TUI as a development preview outside the initial stable package.
- Keep TUI as a peer adapter target; Core contains no terminal or SDL APIs.
- Remove the Backend `cdin_protocol` target dependency. Frontend implements
  the normative wire contract locally as allowed by ADR-0018.
- Pin `m-mdy-m/log.c` v0.1.0 as the only external library dependency; do not
  use the Kernel logger or Kernel package.
- Distinguish the accepted interactive TUI target (FE-009/FE-011) from the
  deferred non-interactive CLI (FE-008). The TUI implementation remains
  partial.
- Expand architecture, module-boundary, Protocol migration, capability,
  semantic-model, text-presentation, and event-loop documentation against
  CDIN Spec 1.2.0.
- Remove the obsolete local TUI proposal as an authority and link to the
  accepted Spec ADR-0020.

### Security

- Keep Frontend independent of Backend, Runtime, and Kernel private headers;
  consume editor behavior only through the versioned Frontend Protocol.

[Keep a Changelog]: https://keepachangelog.com/en/1.1.0/
[Semantic Versioning]: https://semver.org/spec/v2.0.0.html
