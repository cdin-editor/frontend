# Frontend documentation

This is the implementation design and verification guide for
`cdin-editor/frontend`. The cross-project source of truth is [CDIN Spec 1.2.0](https://github.com/cdin-editor/spec).

## Read first

1. [Architecture overview](architecture/overview.md) — what belongs in this repository.
2. [Module boundaries](architecture/module-boundaries.md) — which module may depend on which.
3. [Core API 1.0.0](contracts/core-api.md) — stable UI-neutral Foundation package boundary.
4. [Frontend Protocol](contracts/frontend-protocol.md) — shared 1.0.0 wire contract and client behavior.
5. [Semantic model](contracts/semantic-model.md) — snapshots, `ui.flush`, layout, options and versioned line data.
6. [Implementation status](status.md) — what the source actually does today.

## Contracts

| Document | Defines |
| --- | --- |
| [Core API 1.0.0](contracts/core-api.md) | Stable installed C API, ownership, compatibility and blocking I/O contract. |
| [Frontend Protocol](contracts/frontend-protocol.md) | Negotiation, sequencing, framing, roles, messages and compatibility. |
| [Capabilities](contracts/capabilities.md) | Registered capability names and the distinction between requested and accepted capabilities. |
| [Semantic model](contracts/semantic-model.md) | Runtime UI snapshot, layout tree, typed options, panels and atomic updates. |
| [Text presentation](contracts/text-presentation.md) | Byte-safe rendering and mapping between logical positions and visual coordinates. |
| [Event loop and I/O](contracts/event-loop-and-io.md) | Readiness-driven loop, partial I/O and terminal restoration. |
| [Diagnostics](contracts/diagnostics.md) | Per-component logger ownership and message conventions. |

## Frontend adapters

| Adapter | Status | Document |
| --- | --- | --- |
| `headless` | Partial: bounded trace recorder/replayer; live reference peer pending | [Headless](frontends/headless.md) |
| `tui` | Partial: renderer/input, readiness coordinator and runnable read-only preview | [TUI](frontends/tui.md) and [TUI implementation plan](frontends/tui-implementation-plan.md) |
| `gui-sdl` | Partial: isolated SDL3 smoke preview; production adapter pending | [GUI-SDL](frontends/gui-sdl.md) |
| `cli` | Deferred non-interactive client | [CLI status](frontends/cli.md) |

## Engineering and delivery

- [Implementation roadmap](development/implementation-roadmap.md)
- [Build and test](development/build-and-test.md)
- [Conformance vectors](testing/conformance.md)
- [Verification strategy](testing/verification.md)
- [Requirement traceability](traceability.md)
- [Troubleshooting](troubleshooting.md)
- [UI preview validation](validation-ui-preview.md)

## Status language

- `IMPLEMENTED`: present in this source snapshot and covered by the named tests.
- `PARTIAL`: some behavior exists, but one or more contract requirements or important cases remain.
- `DESIGNED`: the target is documented but not implemented.
- `NOT IMPLEMENTED`: no implementation is present in this repository.
- `BLOCKED`: work cannot be considered complete until a named external component or contract is updated.

These labels describe source status. They do not replace Spec labels such as
`ACCEPTED`, `PROPOSED` or `DEFERRED`.

## Frontend ADRs

- [FE-ADR-0001: Foundation Core API 1.0.0](adr/ADR-0001-foundation-core-api-1.0.0.md) — stable Core package boundary, independent Protocol versioning and explicit preview scope.

## Validation record

[Foundation Core API 1.0.0 validation](FOUNDATION-1.0.0-VALIDATION.md) records the local builds, tests, package-consumer smoke test and limitations for this release boundary.
