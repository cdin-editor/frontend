# Build and test

## 1. Requirements

- C11 compiler (GCC or Clang; Windows uses the supported MinGW-w64 UCRT64
  toolchain).
- CMake 3.22 or newer and GNU Make.
- Bash, Git and network access for first logger dependency installation.
- Pinned `m-mdy-m/log.c` source package from `deps.lock`.

Core/TUI source tests do not require a Backend, Kernel, Runtime, Lua or SDL
checkout. The production composition root supplies transport at runtime. The
stable installed package is Core-only; TUI is a preview build target.

## 2. First build

```sh
git clone https://github.com/cdin-editor/frontend.git
cd frontend
make deps
make check
```

## 3. Targets and current scope

| Target | Purpose | Current status |
| --- | --- | --- |
| `cdin::frontend_core` | Stable Foundation API 1.0.0: Protocol 1.0.0 codec/client, session, capability registry, model metadata and diagnostics | Implemented and installed/exported |
| `cdin::frontend_ui_tui` | Experimental terminal adapter preview | Build/test target only; not installed/exported; no production event loop/app |
| `cdin::frontend_ui_headless` | Recorder/replayer reference frontend | Planned; no target source yet |
| `cdin::frontend_ui_gui_sdl` | SDL3 desktop adapter | Planned; no target source yet |

Core-only and TUI configurations are supported. GUI/Headless CMake options
must remain disabled until their real source and tests exist. Do not make a
configure option appear complete by creating an empty target.

## 4. Test-only logger shim

For an isolated build with no dependency download:

```sh
cmake -S . -B build-test \
  -DCDIN_FRONTEND_USE_LOG_STUB=ON \
  -DCDIN_FRONTEND_BUILD_UI_TUI=ON
cmake --build build-test --parallel
ctest --test-dir build-test --output-on-failure
```

The shim is for tests only. It must never be installed, exported or used for
release packaging. Production builds must resolve the real pinned logger.

## 5. Checks

```sh
make test
make boundary
make lint
make structure   # requires psx
make check
```

CTest groups are organized under `tests/core`, `tests/ui` and future
`tests/e2e`. Shared Protocol vectors are canonical in Spec/its conformance
corpus; local unit tests do not replace them.

## 6. Package consumption

Foundation 1.0.0 exports `cdinFrontend` and the stable target
`cdin::frontend_core` only. Its public headers are installed under
`cdin/frontend/core/`. The TUI preview can be built from this source tree but
is intentionally not exported or installed; it has no 1.0.0 API guarantee.
Consumers must not rely on TUI headers from the installed Foundation package.

## 7. Repository workflow

- Keep CMake source lists aligned with the actual implementation status.
- Run the boundary checker after adding a new platform or UI module.
- Run Pipeline tasks locally where available; CI is not a substitute for local
  CTest and source-boundary checks.
- Use PSX for structure validation and gix for branch-flow where installed.
- Update `docs/status.md` and `docs/traceability.md` with evidence after every
  meaningful implementation milestone.
