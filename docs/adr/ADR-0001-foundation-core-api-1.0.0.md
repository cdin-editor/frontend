# ADR-0001: Stabilize the UI-neutral Frontend Foundation Core API as 1.0.0

- **ID:** FE-ADR-0001
- **Status:** `ACCEPTED`
- **Date:** 2026-11-04

## Context

The Frontend repository needs a stable, reusable foundation for independent
TUI, GUI-SDL and Headless clients. The cross-project Spec accepts Frontend
Protocol `1.0.0` as the next wire contract, but the inspected Frontend and
Backend implementations still speak Protocol `0.2.0`. The TUI is only a
partial adapter; GUI-SDL and Headless are not implemented.

Those facts do not require the UI-neutral Core API to remain unversioned, but
they do prohibit treating a Core package release as proof that all adapters or
the wire protocol are complete.

## Decision

1. Release the UI-neutral Core public C API as **Frontend Foundation 1.0.0**.
2. Version the Foundation package independently from Frontend Protocol,
   Runtime Host API, Kernel ABI and CDIN Spec.
3. Install/export only `cdin::frontend_core` in Foundation 1.0.0. TUI remains a
   buildable development preview and is neither installed nor exported. GUI-SDL
   and Headless are not part of this package release.
4. Freeze the public Core header surface, existing function signatures,
   constants, enum numeric values, structure layouts, ownership rules and
   documented callback behavior throughout Foundation major version 1. A
   change to an existing public structure/signature/layout requires a major
   version increment. Minor releases may add new symbols/types without
   changing existing public layouts.
5. Keep Protocol support explicitly at `0.2.0`. Do not update the Protocol
   version constants to `1.0.0` until Frontend and Backend both implement the
   normative Spec schemas and pass the same conformance corpus.
6. Keep the Core transport callback contract blocking in this release. A
   readiness-driven `AGAIN`/wait API remains a future contract; callers must
   not invoke blocking receive on a UI thread that must process device events.
7. Treat capability registry data and requested/accepted masks as Core
   metadata only. An adapter may request only capabilities it can actually
   provide, and it may use only those accepted for its current connection.

## Supersession note

The Protocol 0.2.0 decision in item 5 was superseded by [FE/BE-ADR-0002](ADR-0002-protocol-1.0.0.md) on 2026-11-04. This ADR remains authoritative for the Foundation Core API 1.0.0 package surface and compatibility policy.

## Implementation update — 2026-11-04

The later [FE/BE-ADR-0002](ADR-0002-protocol-1.0.0.md) supersedes the old
Protocol 0.2.0 decision. The current Frontend codec/client/session implement
Protocol 1.0.0. This does not change the stable Foundation Core API boundary:
readiness-driven I/O is still not added to existing public Core structures,
and TUI/Headless remain preview targets rather than installed Foundation APIs.

## Consequences

- `CMakeLists.txt` and the installed CMake package report Foundation `1.0.0`.
- `version.h` exposes Foundation and Protocol versions separately.
- The Core capability registry and session negotiation helpers are part of
  the stable Foundation API.
- The Core semantic-model helper remains version/flush bookkeeping, not a full
  `cdin.ui-model.v1` model or renderer.
- Foundation 1.0.0 is a stable library building block, not a complete Frontend
  application release. Protocol 1.0.0, async readiness I/O, production TUI,
  Headless, GUI-SDL, transport and composition-root E2E remain separate gates.

## References

- CDIN Spec 1.2.0: ADR-0020, ADR-0018, `frontend-protocol.md`,
  `protocol-wire.md`, `ui-model-v1.md`.
- [Core API contract](../contracts/core-api.md)
- [Implementation status](../status.md)
