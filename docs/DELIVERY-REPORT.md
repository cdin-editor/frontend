# Frontend Foundation 1.0.0 delivery report

This delivery promotes the UI-neutral Core API to Foundation 1.0.0 while
preserving the strict distinction between the Core package version and the
Frontend Protocol wire version.

## What changed

- Set the CMake project/package version to 1.0.0 and add independent compile-time
  Foundation/Protocol version constants.
- Freeze and document the public Core C headers, ownership, threading, blocking
  callback I/O, compatibility and installed-package surface.
- Install/export `cdin::frontend_core` only. TUI remains an experimental
  build/test preview and is not part of the installed package.
- Add the central capability registry, UTF-8/duplicate validation, unknown
  capability handling and requested/accepted capability session support.
- Limit Core view-version bookkeeping to the Spec semantic-model limit of 32.
- Record the release decision in `docs/adr/ADR-0001-foundation-core-api-1.0.0.md`.

## Explicitly not claimed

- A complete semantic UI model/projector or a production/runnable TUI application.
- A complete Headless reference peer; the bounded trace primitive is implemented,
  but live message capture, scripted input and shared-vector conformance remain.
- A concrete platform readiness bridge or the SDL GUI adapter.
- Concrete socket/pipe transport or live Frontend-to-Backend-to-Runtime-to-Kernel
  product integration.

## Verification

- GCC 14.2 Release and Clang 17 Release builds with the test logger package:
  passed.
- CTest: 14/14 passed with the default TUI and Headless targets enabled on
  both compilers.
- Core/UI/dependency boundary checker: passed.
- CMake install/export and a separate consumer build/run passed with a
  temporary logger test-stub package; production logger linkage is still not
  verified in this environment.
- AddressSanitizer + UndefinedBehaviorSanitizer build and tests: 14/14 passed
  with leak detection enabled.
- Markdown lint: not run locally because `markdownlint-cli2` was unavailable;
  local relative-link and whitespace checks passed.
- Production `m-mdy-m/log.c` dependency installation/build: not run; GitHub DNS
  resolution failed in this environment.
- ThreadSanitizer, fuzzing, Windows/macOS native runs, shared cross-repository
  conformance vectors, complete Headless/GUI peers and product E2E: not run or
  not implemented.

The test logger shim is test-only and is not installed or exported by the
production package. The 1.0.0 tag/release should be created only after the
production logger dependency and repository CI pass on the committed tree.

See [FOUNDATION-1.0.0-VALIDATION.md](FOUNDATION-1.0.0-VALIDATION.md) for the consolidated validation record.


## Follow-up implementation — bounded semantic snapshot core

The follow-up adds `include/cdin/frontend/core/model/semantic.h` and
`core/src/model/semantic.c`, with `tests/core/unit/semantic_model.c`.
The new API stores bounded semantic values only; it does not calculate
terminal cells or desktop pixels. Validation covers the 32-view, 16-panel,
63-layout-node and 128-style limits, stable unique identifiers, view placement,
leaf active-view membership, split ratios/axes, selection and scroll bounds,
typed option ranges, and atomic monotonic replacement. A failed validation
leaves the previously committed snapshot unchanged.

The semantic-model follow-up test suite passed 15/15 under GCC Release, Clang
Release and AddressSanitizer + UndefinedBehaviorSanitizer at that checkpoint. The test logger shim is used;
this does not verify the production `m-mdy-m/log.c` dependency.

This is an incremental completion of the semantic-model foundation, not the
full FE-004/FE-005/FE-013 delivery. Protocol message projection, request-ID
correlation for visible line windows, widget schema/renderer, runnable TUI,
SDL GUI and live `cdin` transport/E2E remain release blockers.


## Follow-up implementation — readiness-driven Protocol I/O

Added `include/cdin/frontend/core/protocol/async_client.h` and
`core/src/protocol/async_client.c` as an additive API so the existing blocking
Foundation client remains source/ABI-compatible. The new client preserves
partial receive and send offsets across event-loop turns, enforces frame bounds,
validates Protocol version/direction/sequence, and returns explicit `AGAIN`
without conflating it with EOF. The integration test injects partial progress,
repeated `AGAIN`, truncated-prefix EOF and oversized frame lengths.

The follow-up suite passes 16/16 under GCC Release, Clang Release and
AddressSanitizer + UndefinedBehaviorSanitizer. See
[`validation-followup.md`](validation-followup.md). The concrete platform wait
bridge, async session state machine, live TUI pump and real transport remain
unimplemented.


## Follow-up implementation — runnable UI previews and build workflow

- Added `apps/tui/main.c`, a runnable read-only TUI preview using the existing terminal lifecycle, cell renderer and incremental input parser. It exercises terminal restoration and can be launched with `make run-tui`; it is deliberately not described as an editor because it has no live Protocol transport or Runtime session.
- Added `ui/gui-sdl/app/main.c`, an isolated SDL3 3.2+ desktop smoke preview. SDL usage remains inside `ui/gui-sdl`; diagnostics use the Frontend-owned `m-mdy-m/log.c` facade. The preview is not a production GUI peer and advertises no capabilities.
- Added explicit Makefile commands: `make build`, `make build-tui`, `make run-tui`, `make build-gui`, `make run-gui`, `make test`, and `make help`. `LOGGER=stub` is documented for offline testing only; production builds continue to use the pinned logger.
- Updated Spec traceability, release gate, architecture/build documentation and static boundary checks.
- GCC Release build passed with the test logger; CTest 16/16 passed; the TUI preview passed a pseudo-terminal launch/quit smoke test; boundary checks passed. SDL3 is not installed in this environment, so the GUI target could not be compiled or executed here.
