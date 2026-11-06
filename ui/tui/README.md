# Terminal UI adapter

The TUI is the first interactive Frontend target. It owns terminal input,
terminal-cell layout and terminal rendering. It does not own documents,
commands, keymaps, Runtime policy or Kernel state.

## Current implementation

- Incremental terminal input parsing and physical key/text distinction.
- Terminal lifecycle adapter for POSIX and Windows. On Windows, native console
  handles are used when standard streams are consoles; ConPTY or redirected
  standard streams fall back to `CONIN# Terminal UI adapter

The TUI is the first interactive Frontend target. It owns terminal input,
terminal-cell layout and terminal rendering. It does not own documents,
commands, keymaps, Runtime policy or Kernel state.

## Current implementation

- Incremental terminal input parsing and physical key/text distinction.
/`CONOUT# Terminal UI adapter

The TUI is the first interactive Frontend target. It owns terminal input,
terminal-cell layout and terminal rendering. It does not own documents,
commands, keymaps, Runtime policy or Kernel state.

## Current implementation

- Incremental terminal input parsing and physical key/text distinction.
 handles and restore their
  original modes before closing only handles owned by the adapter.
- POSIX input reads probe process STDIN with a zero-timeout `poll` before
  each read. The drain callback is nonblocking without opening/duplicating the
  terminal or changing STDIN/STDOUT flags; renderer writes remain blocking.
  The standalone preview waits for input readiness before reading.
- Read-only single-view rendering and a version-aware presentation cache.
- A readiness-driven event-loop coordinator (`event_loop.h`).

The renderer includes a bounded, cluster-aware width projection for combining
marks, emoji variation selectors/modifiers, regional-indicator pairs and ZWJ
emoji sequences. Original UTF-8 bytes are preserved. This is not complete
UAX #29 segmentation, script shaping or bidi reordering.

The event-loop coordinator is deliberately transport-agnostic. When a
termination and Backend-readiness event arrive together, it gives the already
ready Backend source one bounded drain turn before calling shutdown; it does
not accept fresh terminal input after termination has been requested. `cdin` supplies
the wait callback that multiplexes terminal readiness, Backend transport
readiness, resize and termination events. Drain callbacks must be nonblocking,
return `AGAIN` when empty and obey the per-source item budget. The coordinator
services both ready sources in each turn and renders only after model or
geometry changes.

## Not yet a complete editor

This library is not a runnable editor application. It still needs the actual
`cdin` composition-root transport and Backend pump, atomic semantic snapshot
application, full PaneTree and panel rendering, complete Unicode grapheme and
bidi mapping, visible-line request scheduling, and PTY/console lifecycle tests.
Do not describe this target as a shipped Neovim-like editor until those gates
pass.
