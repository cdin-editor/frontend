# UI preview build validation

**Spec references:** FE-009/FE-011 (TUI first interactive target), FE-019
(readiness-driven TUI lifecycle), FE-020 (SDL confined to GUI-SDL), and FE-010
(peer-adapter repository topology).

## Commands executed

```sh
make LOGGER=stub BUILD_DIR=build-new build
make LOGGER=stub BUILD_DIR=build-new test
make boundary
```

Results in this environment:

- GCC 14.2 Release: Core, TUI library, Headless library, tests and the TUI
  preview executable built successfully.
- Clang 17 Release: same source targets built successfully.
- CTest: 16/16 passed under GCC and Clang.
- TUI preview pseudo-terminal smoke test: PASS; it entered the alternate
  screen, rendered the preview, accepted `q`, exited and restored terminal state.
- Static Core/UI/peer/logger boundary check: PASS, including the new rule that
  SDL symbols and headers may occur only inside `ui/gui-sdl`.
- Markdown lint: skipped because `markdownlint-cli2` is not installed.

## GUI-SDL limitation

The GUI source and CMake target are present, but SDL3 3.2+ development files are
not installed in this environment. Configuring with
`CDIN_FRONTEND_BUILD_UI_SDL=ON` therefore fails at `find_package(SDL3 3.2)` as
expected. The GUI executable has **not** been compiled or run here. On a machine
with SDL3 3.2+ installed, verify it with:

```sh
make LOGGER=stub build-gui
make LOGGER=stub run-gui
```

For release builds, omit `LOGGER=stub` and install the pinned production
`m-mdy-m/log.c` dependency with `make deps`.

## Product boundary

The TUI and GUI executables are smoke previews. Neither creates a live Frontend
Protocol session or edits a document. The production `cdin` composition root
must supply transport/readiness integration, and Runtime remains authoritative
for commands, keymaps and semantic editor state. These previews do not close
the FE-004/FE-005/FE-013/FE-019/FE-023 or live E2E release gates.
