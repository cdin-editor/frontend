# Frontend Foundation 1.0.0 — validation record

**Validation date:** 2026-11-04
**Scope:** stable, UI-neutral `cdin::frontend_core` package plus optional TUI/Headless development-preview build and tests; not the complete Frontend product.
**Protocol:** the current implementation is Frontend Protocol `1.0.0`.

This record reports commands executed against the working tree prepared for
Foundation Core API 1.0.0. It is not a claim that GUI-SDL, the TUI application, Headless, or the complete
CDIN application is ready.

## Passed

| Check | Result | Scope / notes |
|---|---|---|
| Strict Linux Debug build | PASS | GCC 14.2.0; C11 warnings treated as errors; test logger shim enabled. |
| Original Foundation baseline | PASS, 12/12 | Historical Core + TUI preview result from the initial Foundation validation. |
| Current GCC 14.2 Release CTest | PASS, 14/14 | Includes the readiness coordinator and bounded Headless trace recorder/replayer tests. |
| Current Clang 17 Release CTest | PASS, 14/14 | Same 14-test suite under Clang 17. |
| Current ASan + UBSan CTest | PASS, 14/14 | Full current suite with leak detection and halt-on-error enabled. |
| Core-only CTest | PASS, 7/7 | TUI disabled; stable installed target source and tests only. |
| ASan + UBSan Core-only CTest | PASS, 7/7 | AddressSanitizer and UndefinedBehaviorSanitizer; leak detection and halt-on-error enabled. |
| Dependency / UI boundary checker | PASS | `bash scripts/check-boundaries.sh`. |
| CMake install/export | PASS | Installs Core headers and exports `cdin::frontend_core`; TUI is excluded. |
| Independent CMake consumer | PASS | `find_package(cdinFrontend 1.0 CONFIG REQUIRED)`, links `cdin::frontend_core`, checks version and capability lookup, then runs. |
| Version consistency | PASS | CMake project, public Foundation version header and Protocol version header are cross-checked by a compile-time/runtime unit test. |
| Local Markdown links | PASS | All relative Markdown links resolve to an existing file (zero broken paths). |

## Test dependency substitution

The local test environment could not download the pinned production
`m-mdy-m/log.c` release because GitHub DNS resolution failed. Core unit tests
and the separate install/consumer smoke test therefore used the repository's
test-only logger ABI shim. The shim is not installed or exported by the
production package. The production logger integration must still pass the
repository CI build before a public release is tagged.

## Not run / not implemented

- Markdown lint: `markdownlint-cli2` was not installed in this environment.
- ThreadSanitizer and sustained fuzz campaigns.
- Native macOS and Windows builds (the Windows TUI terminal implementation was
  not executed locally).
- Production `m-mdy-m/log.c` install/build.
- The complete Spec-owned Protocol `1.0.0` vector corpus; targeted paired
  positive/negative regressions for typed options and nullable `active_view`
  are now run in both Backend and Frontend suites.
- A runnable TUI application, GUI-SDL, complete live Headless reference peer,
  concrete socket/pipe transport, and full Frontend → Backend → Runtime → Kernel E2E.
- Production logger dependency and native Windows/macOS test runs remain unverified.

## Release interpretation

The checks above support the stability claim for the installed UI-neutral
Foundation Core API `1.0.0` and the source-level Protocol `1.0.0` codec/session
implementation. They do not justify a complete Frontend application release,
full cross-repository conformance, or deletion of the legacy CDIN composition
root. Those require the remaining gates in [status.md](status.md).
