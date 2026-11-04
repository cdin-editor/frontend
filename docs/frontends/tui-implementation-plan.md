# TUI implementation plan

**Product decision:** TUI is the first interactive Frontend target and is
accepted by Spec FE-009/FE-011/FE-019 and ADR-0020. This plan is implementation
work; it does not mark unfinished code as complete.

## T0 — Protocol 1.0.0 alignment

**Status: codec/client/session alignment implemented; full model and shared
conformance corpus remain open.**

- Frontend and paired Backend source snapshots implement Protocol 1.0.0.
- Frontend validates typed `ui.options.options` and nullable `active_view`.
- Core capability registry distinguishes requested and accepted sets.
- Envelope sequence, flush sequence, model revision and document version are
  represented independently.
- Full PaneTree/panel semantic-model retention and exhaustive shared vectors
  are still required; codec validation alone does not make the TUI complete.

## T1 — Readiness-driven application loop

**Status: transport-agnostic coordinator implemented; platform and transport
integration remain open.**

- `ui/tui/src/session/event_loop.c` requires a blocking readiness wait supplied
  by the composition root and never polls or sleeps.
- Terminal and Backend drain callbacks are nonblocking, return explicit
  `PROGRESS`/`AGAIN`/EOF/error results, and are bounded per turn.
- Simultaneously ready terminal and Backend sources are both serviced before
  rendering; resize and termination events are routed through callbacks.
- Still required: concrete POSIX/Windows wait bridge, readiness-driven partial
  Protocol I/O, ESC deadline integration and orderly signal cleanup.
- Keep Backend pump/Runtime/Lua dispatch on the Backend owner thread.

**Exit:** fake coordinator test passes; product exit still requires PTY/console
and fake-transport tests proving neither side blocks the other, no busy wait,
no unbounded queue and no `AGAIN`/EOF confusion.

## T2 — Reliable one-view editor path

- Complete the multi-view snapshot/response transaction and ACK-after-commit.
- Single-active-view exact line-request correlation and stale/future-response rejection are implemented; add concurrent per-view request tracking.
- Finish option, style, status, prompt, cursor and selection handling.
- Confirm raw key/text/paste mapping and driver permissions.
- Integrate Runtime default commands/keymaps through the Backend event path.

**Exit:** one terminal view can open, navigate, insert, delete, undo/redo and
save through Runtime without Frontend-owned editor policy.

## T3 — Layout and declarative panels

- Parse and validate the complete logical pane tree.
- Map split ratios to cell rectangles using a deterministic resize policy.
- Support empty panes and nullable active view.
- Render semantic style roles and declarative panel/widget state.
- Handle snapshot removal of omitted panes/panels atomically.

**Exit:** several views/panels work under the Spec limits and resize without
stale geometry or document-byte changes.

## T4 — Text fidelity

- Implement byte-to-code-point-to-grapheme-to-cell mapping.
- Add width handling for combining marks, wide characters and emoji sequences.
- Implement visual-line movement and wrap-aware cursor placement.
- Define and test terminal-specific bidi behavior for Persian/Arabic and mixed
  RTL/LTR content.
- Never normalize or rewrite original line bytes.

**Exit:** shared coordinate invariants pass, and terminal support is documented
by emulator/font/OS matrix. Do not claim universal RTL correctness without
matrix evidence.

## T5 — Terminal capability integration

- Implement and advertise mouse only when enabled and tested.
- Add clipboard/dialog/truecolor/IME only when the terminal adapter can really
  service them and Backend accepts them.
- Keep `canvas` optional and absent by default.
- Test reconnect and changed capability sets.

**Exit:** every advertised capability has a concrete implementation and tests;
unsupported features degrade explicitly.

## T6 — Production hardening

- Bound every pending frame, line cache, parser, redraw batch and queue.
- Add fault injection for partial writes, EOF, queue pressure and malformed
  messages.
- Verify terminal restore on every recoverable exit/error path.
- Add startup, steady-state memory and latency measurements.
- Run Linux and Windows native integration; add macOS POSIX coverage where the
  platform adapter claims support.
- Complete the real composition-root E2E path and release checklist.

**Exit:** no known safety/data-loss issues; full supported platform matrix and
release gates are green.

## Dependencies outside Frontend

- Backend must implement and advertise Protocol 1.0.0 only after its validator,
  projector, negotiation and conformance tests pass.
- Runtime must provide the accepted `cdin.ui-model.v1` projection and default
  editing commands/keymaps through its Host API.
- `cdin` must supply the concrete byte transport, readiness integration,
  process lifecycle and coordinated Backend pump.
- Kernel remains behind Backend; TUI never links or calls Kernel.

## Explicit non-goals for the initial release

- A second command interpreter or Vim keymap implementation in C.
- A new canonical document buffer, file loader or persistence subsystem in
  TUI.
- A universal terminal-specific image/canvas protocol.
- A claim of identical rendering across all terminal emulators.
- A requirement that TUI and GUI share text or rendering code.
