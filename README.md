<p align="center">
  <a href="https://github.com/cdin-editor/cdin-editor.github.io/tree/main/public/cdin-icons" target="_blank" rel="noreferrer">
    <img src="https://raw.githubusercontent.com/cdin-editor/cdin-editor.github.io/main/public/cdin-icons/source/cdin-original-transparent-512.png" width="180" alt="CDIN logo" />
  </a>
</p>
<p align="center">
  <a href="https://github.com/cdin-editor/frontend/actions/workflows/ci.yml"><img src="https://github.com/cdin-editor/frontend/actions/workflows/ci.yml/badge.svg" alt="CI status"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPL--3.0-blue.svg" alt="License: GPL-3.0-or-later"></a>
  <a href="https://en.cppreference.com/w/c/11"><img src="https://img.shields.io/badge/Language-C11-blue.svg" alt="C11"></a>
  <a href="https://github.com/cdin-editor/spec"><img src="https://img.shields.io/badge/CDIN-Specification-1.2.0-purple.svg" alt="CDIN specification"></a>
</p>

<h1 align="center">CDIN Frontend</h1>

<p align="center">Independent protocol clients and environment-specific presentation for CDIN.</p>

CDIN Frontend is the presentation side of the editor. It consumes the semantic
Frontend Protocol, translates physical device events into protocol input, and
renders the resulting UI model for a terminal, desktop window, or headless test
harness. It is not an editor-policy layer and it is not a second document
engine.

## Architectural boundary

The Frontend is a peer of the Backend. Its only editor-facing contract is the
versioned [Frontend Protocol](https://github.com/cdin-editor/spec/blob/main/docs/architecture/frontend-protocol.md).
The Frontend does not include or link Backend, Runtime, or Kernel private
headers, and it never receives Kernel handles. It owns its own protocol
codec/client implementation and visual presentation state. The `cdin`
composition root supplies the byte-stream transport callbacks and chooses
which frontend adapter to launch.

| Component | Owns |
| --- | --- |
| **Frontend** | Device input, terminal/window resources, visual layout, text presentation, drawing and disposable presentation caches. |
| **Backend** | Sessions, Protocol server, Runtime host, OS services and the direct Kernel adapter. |
| **Runtime** | Commands, keymaps, modes, views, selections, configuration and semantic UI state. |
| **Kernel** | Canonical document bytes, mutations, versions, snapshots, notifications and undo/redo. |
| **cdin** | Composition, transport construction, process lifecycle, build, bundle and launch. |

The current cross-project design authority is [CDIN Spec 1.2.0](https://github.com/cdin-editor/spec). A requirement marked `ACCEPTED` in Spec records a decision; it does not mean this repository has implemented it. See [implementation status](docs/status.md) and [traceability](docs/traceability.md).

## Build and test

Requirements: C11 compiler, CMake 3.22+, GNU Make, Git, Bash and network access
for the first installation of the pinned logger dependency.

```sh
git clone https://github.com/cdin-editor/frontend.git
cd frontend
make build                 # Core + TUI preview + Headless
make run-tui               # launch the read-only terminal preview
make build-gui             # requires SDL3 >= 3.2
make run-gui               # launch the SDL3 desktop preview
make test
```

The default build uses the pinned production `m-mdy-m/log.c` dependency. For
offline source work only, use `LOGGER=stub` (never for release artifacts):

```sh
make LOGGER=stub build
make LOGGER=stub test
```

To include GUI-SDL in the ordinary build, use `make GUI=ON build`; SDL3 3.2+
must already be discoverable by CMake. Focused checks:

```sh
make build-core
make build-tui
make boundary
make lint
make structure  # requires psx
```

For isolated source tests without downloading the logger, the test-only shim
can be enabled explicitly:

```sh
cmake -S . -B build-test \
  -DCDIN_FRONTEND_USE_LOG_STUB=ON \
  -DCDIN_FRONTEND_BUILD_UI_TUI=ON \
  -DCDIN_FRONTEND_BUILD_UI_HEADLESS=ON
cmake --build build-test --parallel
ctest --test-dir build-test --output-on-failure
```

The shim is not a production dependency and must never be used for a release
artifact. Production installation exports `cdin::frontend_core` and, when TUI
is enabled, `cdin::frontend_ui_tui` with public headers. When
`CDIN_FRONTEND_BUILD_UI_SDL=ON` and SDL3 is available, it also exports
`cdin::frontend_ui_gui_sdl`; the preview executable remains a separate
development app. An exported adapter is not a claim that the full desktop
editor is release-ready. Frontend PR #17 was merged at
[`fea9560`](https://github.com/cdin-editor/frontend/commit/fea9560d60085aa4d579efc6a584d90349ee4ac0).
A follow-up review/fix branch now covers ConPTY console-handle acquisition,
strict snapshot revisions, request-correlated visible-line responses, GUI
pixel-session validation, foreign-window SDL event filtering and regression
coverage; its native/SDL CI is pending before it is integrated. CDIN PR #110's
GUI-SDL live Backend/Runtime smoke passed on Windows, but its separate
TUI ConPTY smoke exposed redirected standard handles being treated as console
handles; the Frontend follow-up addresses that adapter boundary. The renderer
is still an ASCII/debug-text baseline; full text shaping, bidi/Persian rendering,
IME, complete PaneTree presentation and product-ready desktop behavior remain
open. See [Build and test](docs/development/build-and-test.md) and the
[GUI-SDL implementation](docs/frontends/gui-sdl.md).

## Documentation

| Guide | Purpose |
| --- | --- |
| [Architecture overview](docs/architecture/overview.md) | Product role, target topology and ownership. |
| [Module boundaries](docs/architecture/module-boundaries.md) | Allowed dependencies and forbidden coupling. |
| [Core API 1.0.0](docs/contracts/core-api.md) | Stable installed Core C API, ownership, compatibility and I/O rules. |
| [Frontend Protocol](docs/contracts/frontend-protocol.md) | Shared Protocol 1.0.0 wire contract and client behavior. |
| [Capabilities](docs/contracts/capabilities.md) | Registry, negotiation and per-adapter support. |
| [Semantic model](docs/contracts/semantic-model.md) | Atomic snapshots, options, layout, panels and versioned lines. |
| [Text presentation](docs/contracts/text-presentation.md) | Byte-to-visual mapping, Unicode, graphemes, bidi and Persian. |
| [Event loop and I/O](docs/contracts/event-loop-and-io.md) | Readiness-driven loop and terminal/network multiplexing. |
| [Diagnostics](docs/contracts/diagnostics.md) | Logger ownership, levels and record format. |
| [TUI](docs/frontends/tui.md) | Interactive terminal frontend and implementation plan. |
| [GUI-SDL](docs/frontends/gui-sdl.md) | Desktop frontend design. |
| [Headless](docs/frontends/headless.md) | Protocol reference recorder/replayer. |
| [Implementation roadmap](docs/development/implementation-roadmap.md) | Ordered work packages and acceptance gates. |
| [Verification](docs/testing/verification.md) | Unit, conformance, integration, platform and E2E tests. |
| [Status](docs/status.md) | Implemented, partial and missing work. |
| [Traceability](docs/traceability.md) | Requirement-to-source mapping. |
| [Frontend 1.0.0 release gate](docs/FRONTEND-1.0.0-RELEASE-GATE.md) | FE-by-FE release decision, source evidence, rationale and remaining acceptance tests. |

## License

CDIN Frontend is licensed under **GPL-3.0-or-later**. See [LICENSE](LICENSE)
and [NOTICE](NOTICE).
