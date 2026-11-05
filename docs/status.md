# Implementation and release status

**Source snapshot:** `cdin-editor/frontend` Foundation 1.0.0 working tree,
reviewed 2026-11-04. This status describes code present in this repository. It
does not promote accepted Spec requirements into implementation claims.

## 1. Release scope

Foundation 1.0.0 stabilizes and installs the UI-neutral C Core API:

- independent Protocol codec/client API for the currently implemented wire
  version;
- connection/session state machine;
- registered capability names and requested/accepted set validation;
- snapshot revision, flush sequence and document-version bookkeeping;
- input value types and Frontend-owned diagnostics facade.

The installed CMake package always exports `cdin::frontend_core` and, when
`CDIN_FRONTEND_BUILD_UI_TUI=ON`, also exports the reusable
`cdin::frontend_ui_tui` adapter library and its public headers. The independent
wire codec/client/session implement shared Protocol 1.0.0. Exporting the TUI
library makes it consumable by CDIN; it does not promote the preview app or
FE-019 loop to product-ready status. Headless remains a build/test-only trace
primitive; GUI-SDL has a smoke-preview executable but not a production adapter.

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
| TUI terminal lifecycle | `PARTIAL` | POSIX and Windows terminal open/close code exists; a runnable read-only preview now exercises enter/render/input/restore, but signal-safe lifecycle and readiness integration remain. |
| TUI input parser | `PARTIAL` | Incremental CSI/UTF-8/Escape/paste parsing; complete keyboard/mouse/IME matrix remains. |
| TUI renderer | `PARTIAL` | One-view text/cursor/selection/status rendering. No full PaneTree, style-role, widget or multi-pane support. |
| TUI event loop/application | `PARTIAL` | `cdin::frontend_ui_tui` and its public headers are installed/exported when TUI is enabled; the independent consumer check is part of CI. The preview executable remains read-only. The readiness coordinator is not yet wired to a platform wait bridge or live Backend pump; editor commands remain Runtime-owned. |
| TUI text fidelity | `PARTIAL` | Basic cell output and common-width handling; complete grapheme, shaping, bidi and Persian mapping remain. |
| Headless reference frontend | `PARTIAL` | Bounded validated trace recorder/replayer builds by default; live peer wiring, scripted input harness and shared-vector runner remain pending. Backend's separate test harness is not this adapter. |
| SDL desktop frontend | `PARTIAL — preview only` | Isolated SDL3 3.2+ smoke-preview executable and event loop; production text/font/layout/rendering, semantic widgets, capabilities and live Protocol integration remain. |
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
- TUI read-only preview: pseudo-terminal launch/render/quit/terminal-restore smoke test PASS; GUI-SDL source is present but could not be compiled because SDL3 3.2+ is unavailable here. See [UI preview validation](validation-ui-preview.md).
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
  were added after the original Foundation validation record; see the current
  CTest result and `docs/traceability.md`.

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
   implementation supplied by `cdin`; add native wait and cleanup integration.
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
