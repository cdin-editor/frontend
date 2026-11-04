# Frontend Core

Frontend Core is the stable, UI-neutral C foundation shared by independent
peer adapters. Foundation Core API 1.0.0 is the first stable installed package
and exports `cdin::frontend_core`.

It contains the independent Protocol codec/client, connection/session state,
registered capability identifiers and requested/accepted capability
validation, protocol-level snapshot/version bookkeeping, input value types and
a diagnostics facade.

The current wire implementation is **Frontend Protocol 1.0.0** and follows the
shared contract in `docs/contracts/protocol-1.0.0.md`. Its independent codec is
kept aligned with the Backend implementation through paired positive/negative
protocol tests. End-to-end Runtime and transport integration remains separate.

Core does not own terminal/SDL APIs, font shaping, visual layout, text
presentation, rendering, command policy, keymaps, document bytes or
authoritative selections. Do not add a shared GUI/TUI rendering or text-layout
engine here.

Build target: `cdin::frontend_core`. The target works with all UI adapters
disabled. The installed Foundation package exports Core only. TUI is a
separate development preview, not a stable installed target.
