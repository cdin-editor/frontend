# Implementation and release status

**Source snapshot:** Frontend PR #17 head
`552eedd14e3971e2b39d706804d4af57f8b2dc38`, inspected 2026-11-07.
[CI run 42295241142](https://github.com/cdin-editor/frontend/actions/runs/42295241142)
passed Linux/macOS/Windows and Pipeline quality, the GUI-SDL AddressSanitizer
suite, install/export and clean-prefix consumer. This status describes code
present in the inspected source tree and does not promote accepted Spec
requirements into implementation claims.

## 1. Release scope

Foundation 1.0.0 stabilizes and installs the UI-neutral C Core API:

- independent Protocol codec/client API for the currently implemented wire
  version;
- connection/session state machine;
- registered capability names and requested/accepted set validation;
- snapshot revision, flush sequence and document-version bookkeeping;
- input value types and Frontend-owned diagnostics facade.

The installed CMake package exports `cdin::frontend_core`; it also exports
`cdin::frontend_ui_tui` when TUI is enabled and
`cdin::frontend_ui_gui_sdl` when `CDIN_FRONTEND_BUILD_UI_SDL=ON`. The SDL
adapter is a reusable static library separate from its preview application.
The independent wire codec/client/session implement shared Protocol 1.0.0.
Exporting an adapter makes it consumable by CDIN; it does not make the preview
or full interactive editor product-ready. Headless remains a build/test-only
trace primitive.

Foundation 1.0.0 and the Protocol 1.0.0 Core codec/session contract are versioned and tested at source level. This is **not** a claim that the full Frontend product or complete CDIN application is ready.

## 2. Implementation matrix

| Area | Status | Evidence / remaining work |
|---|---|---|
| Foundation Core public API | `STABLE 1.0.0` | Public headers under `include/cdin/frontend/core/`; package exports only `cdin::frontend_core`. |
| Independent Protocol codec | `IMPLEMENTED 1.0.0` | Deterministic-CBOR 1.0.0 framing, payload and semantic checks, encode/decode, frame bounds and callback-based partial I/O. |
| Protocol 1.0.0 conformance | `IMPLEMENTED AT CORE CODEC LEVEL` | Typed `ui.options.options`, nullable `active_view`, frame/session validation and paired protocol tests; full backend/runtime/transport E2E remains gated. |
| Core client | `IMPLEMENTED` (blocking + additive async) | Legacy blocking API remains ABI-stable; new caller-buffered async API preserves partial RX/TX state, handles `AGAIN`/EOF/error and validates sequence. Concrete transport wait integration remains external. |
| Session lifecycle | `IMPLEMENTED 1.0.0` | Hello/attach/driver/flush ACK/detach and requested/accepted capability state, negotiating exact Protocol 1.0.0. |
| Capability registry | `IMPLEMENTED IN CORE` | Nine registered Spec names, unknown-name ignore, duplicate/malformed rejection, accepted-subset validation. Concrete adapter masks remain empty unless explicitly set. |
| Semantic model | `PARTIAL — expanded` | Added bounded typed snapshot values and allocation-free validation for 32 views, 16 panels, layout tree, typed options, styles, UTF-8 values and non-regressing view/document revisions. Protocol message projection, widget schema and multi-view/concurrent line request tracking remain unimplemented; the TUI now validates its single outstanding request by exact view/version/range identity. |
| TUI terminal lifecycle | `PARTIAL — POSIX READINESS FOLLOW-UP` | POSIX probes process STDIN with zero-timeout `poll` before reading, avoiding any open/dup/fcntl changes to process STDIN/STDOUT flags. The preview waits for input readiness and handles a raced `EAGAIN`. These changes and PTY regressions are in [PR #5](https://github.com/cdin-editor/frontend/pull/5); native macOS verification is still pending. Windows terminal lifecycle code remains present, but the native console path is not yet a proven FE-019 adapter. |
| TUI input parser | `PARTIAL` | Incremental CSI/UTF-8/Escape/paste parsing; complete keyboard/mouse/IME matrix remains. |
| TUI renderer | `PARTIAL` | One-view text/cursor/selection/status rendering. PaneTree semantics are retained transactionally, but cell geometry, multi-pane rendering, style roles and widgets remain. |
| TUI PaneTree model | `PARTIAL` | Parses and atomically commits the Protocol PaneTree (bounded 63 nodes/32 views), nullable roots/active leaves and validated split ratios. Geometry, multi-pane rendering and panel state remain open. |
| TUI event loop/application | `PARTIAL` | `cdin::frontend_ui_tui` and its public headers are installed/exported. The transport-agnostic coordinator has bounded drains and now keeps servicing already-ready Backend work across multiple turns when termination is latched; it suppresses new terminal input during that drain. PR #5 adds regression coverage, but its native matrix has not yet passed. The preview remains read-only, and CDIN still owns the combined terminal/Backend/resize/signal wait bridge and live Backend pump. Editor commands remain Runtime-owned. |
| TUI text fidelity | `PARTIAL — GRAPHEME WIDTH FOLLOW-UP` | [Frontend PR #6](https://github.com/cdin-editor/frontend/pull/6) groups combining marks, variation selectors/modifiers, regional-indicator pairs and ZWJ emoji sequences for cell-width/cursor calculation while preserving source bytes. Full UAX #29 segmentation, shaping, bidi/Persian visual ordering and terminal/font matrices remain open. PR #6 is rebased conflict-free on the PR #5 merge commit; current native/quality CI is pending. |
| Headless reference frontend | `PARTIAL` | Bounded validated trace recorder/replayer builds by default; live peer wiring, scripted input harness and shared-vector runner remain pending. Backend's separate test harness is not this adapter. |
| SDL desktop frontend | `PARTIAL — installable adapter foundation` | `cdin::frontend_ui_gui_sdl` exports public headers/library, forwards physical input and focus/resize, and has a bounded semantic line model. Frontend run 42295241142 passed SDL AddressSanitizer tests and clean-prefix package consumer. Renderer/text/pane/IME capabilities are incomplete; live CDIN Backend/Runtime composition is a separate unverified gate. |
| Concrete transport | `NOT IMPLEMENTED` | `cdin` composition root must supply transport construction, readiness and lifecycle. |
| Live Backend/Runtime/Kernel E2E through a Frontend adapter | `NOT IMPLEMENTED` | Backend ↔ Runtime ↔ Kernel has a successful in-process smoke path, but no production Frontend adapter/transport E2E. |
| Diagnostics | `IMPLEMENTED WITH DEPENDENCY GATE` | Facade uses pinned `m-mdy-m/log.c` 0.1.0. Local isolated tests used a test-only ABI shim; production dependency build was not run here. |

## 3. Independent version matrix

| Contract | Version | Meaning |
|---|---:|---|
| Frontend Foundation Core API | `1.0.0` | Stable installed C API in this release. |
| Frontend Protocol | `1.0.0` | Exact wire implementation supported by current Core codec/client. |
| Shared Frontend Protocol contract | `1.0.0` | Implemented by Frontend and Backend source codecs; end-to-end runtime/transport gates remain. |
| Backend Protocol | `1.0.0` | Matching codec/server and source version in paired snapshot. |
| Runtime Host API | `1.0.0` | Paired Runtime/Backend source archives report 1.0.0; not directly consumed by Frontend. |
| Kernel C ABI | `1.1.0` | Backend-only Kernel boundary. |
| CDIN Spec | `1.2.0` | Cross-project architecture/contract authority. |

Do not infer Protocol support from the Foundation package version. A Foundation package version does not imply a wire version. Both source snapshots currently negotiate Protocol 1.0.0 and use the shared Spec contract; a production connection still requires the Runtime patch, concrete transport and live integration gates.

## 4. Verification completed for this snapshot

See the detailed [Foundation 1.0.0 validation record](FOUNDATION-1.0.0-VALIDATION.md) for command scope and limitations.

- GCC 14.2 Release and Clang 17 Release CMake configure/build: PASS.
- CTest: **16/16 passed** with `CDIN_FRONTEND_USE_LOG_STUB=ON`, TUI preview
  and Headless trace adapter enabled on both compilers. Tests cover logging, capability registry,
  model state, wire codec, blocking and async Protocol clients, session lifecycle, Headless trace
  record/replay and TUI rendering/model/input/session/scroll/event loop.
- Core/UI and dependency boundary checker: PASS, including the SDL-only-in-GUI-SDL rule.
- TUI read-only preview remains separate from the installed reusable TUI adapter and CDIN's experimental TUI process path. Native POSIX and Windows integration acceptance is owned jointly with CDIN and remains release-gated. GUI-SDL source was built with pinned SDL3 3.4.16 in Frontend CI run 42295241142; its model/adapter tests passed under AddressSanitizer and the installed clean-prefix consumer linked successfully. The renderer remains an ASCII/debug-text baseline. See [UI preview validation](validation-ui-preview.md).
- CMake install/export plus a separate consumer configure/build/run: PASS for
  the Foundation Core package in the earlier validation record using a local
  test-stub logger package. The new clean-prefix consumer additionally checks
  the exported TUI target and command API; its CI result is pending for this
  source branch and must not be inferred from the earlier Core-only test.
- AddressSanitizer + UndefinedBehaviorSanitizer build and tests:
  PASS, 16/16, with leak detection enabled and both sanitizers configured to
  halt on the first finding.
- Markdown lint invocation: skipped because `markdownlint-cli2` is not installed
  in this environment; local relative Markdown-link check passed and CI docs
  workflow remains the authoritative lint run.
- Production logger install/build: NOT RUN. `scripts/install-deps.sh` was
  attempted but GitHub DNS resolution failed in this environment.
- Windows terminal code: NOT executed locally.
- ThreadSanitizer, fuzz campaigns, macOS/Windows native builds, SDL3 GUI preview
  (SDL3 unavailable in this environment), live Headless peer wiring and live
  product E2E: NOT RUN / NOT IMPLEMENTED as applicable.
- The optional Headless trace recorder/replayer and TUI readiness coordinator
  were added after the original Foundation validation record. PR #5 is merged;
  the historical 16/16 result does not cover the post-merge terminal and
  shutdown follow-up. Check the current main-branch native matrix before using
  this section as a release verification claim. See `docs/traceability.md`
  and PR #5.

The test-only logger shim is never a production dependency and is excluded
from the installed/exported package.

## 5. Remaining product-release gates

1. Verify the paired Runtime Host API 1.0.0 source against real Lua 5.4 and
   Kernel packages; expand identical positive and negative vectors to every
   Protocol message schema.
2. Complete the Protocol-to-semantic-model projector and atomic flush
   transaction. The bounded value/validator layer already covers 32 views,
   16 panels, PaneTree, typed options, styles, nullable active views and UTF-8;
   it still must be populated from every wire message and unified with the TUI
   model. Declarative widget values and multi-view request scheduling remain.
3. Implement per-adapter capability support and only advertise capabilities
   the adapter can actually service in that process/session.
4. Connect the implemented readiness-driven partial-progress Protocol I/O
   (`PROGRESS`, `AGAIN`, EOF and error) to the concrete terminal/transport wait
   implementation supplied by `cdin`; complete and verify the POSIX/Windows
   wait bridges, signal/resize delivery, pending-work shutdown and cleanup. PR #5
   is merged and supplies the reusable Frontend coordinator and POSIX
   input/preview contract, but does not implement CDIN's combined wait bridge.
5. Turn the TUI read-only preview into a connected interactive editor with
   terminal restoration, multi-pane rendering, text mapping, and Runtime event flow.
6. Complete GUI-SDL rendering, fonts/shaping, semantic widgets and capabilities;
   the current SDL3 executable is only a desktop smoke preview.
7. Wire the Headless trace recorder/replayer to every live peer message,
   add scripted input playback and run the shared Protocol conformance corpus.
8. Complete GUI-SDL as an independent peer, including an approved/pinned
   font, shaping and bidi stack.
9. Connect a real composition-root transport and run live Frontend ↔ Backend ↔
   Runtime ↔ Kernel E2E on supported operating systems.
10. Run production logger builds, full Linux/macOS/Windows CI, sanitizer/fuzz
   coverage and byte-preserving file round-trip tests.

Foundation 1.0.0 may be used as the stable Core building block for this work;
these gates prevent the repository from claiming a complete Frontend product
release or a production-ready CDIN rewrite.
