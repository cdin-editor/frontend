# TUI — interactive terminal frontend

**Spec status:** accepted first interactive Frontend target (FE-009, FE-011,
FE-019; ADR-0020). **Implementation status:** early one-view terminal library;
not yet a complete runnable editor application. See [status](../status.md).

## 1. Product intent

The TUI is a full-screen terminal editor with a Neovim-like information
hierarchy and modal editing experience. "Neovim-like" describes the product
interaction style and screen organization; it does not authorize the TUI to
reimplement Vim modes, commands or keymaps. Runtime owns those semantics.

The TUI is not the non-interactive `cli`. CLI remains a separate deferred
client described in [cli.md](cli.md).

## 2. UI composition

```text
┌ CDIN · workspace · current view ───────────────────────────────────────────┐
│ [buffer / view labels]                                      optional tabs  │
├───────────────────┬────────────────────────────────────────────────────────┤
│ declarative panel │  editor pane / logical view                            │
│ or explorer       │  1  source bytes rendered as terminal cells             │
│                   │  2  semantic styles and selection                       │
│                   │  3  cursor mapped from byte offset to display cell      │
│                   │  4                                                       │
│                   ├────────────────────────────────────────────────────────┤
│                   │  optional second logical pane                          │
├───────────────────┴────────────────────────────────────────────────────────┤
│ mode · status · document metadata · cursor information                     │
│ prompt / command input / completion / notification                         │
└────────────────────────────────────────────────────────────────────────────┘
```

The actual widgets and visible regions come from the semantic UI model:

| Region | Source | TUI responsibility |
| --- | --- | --- |
| Header/tabline | `ui.options`, status and declarative panels | Arrange labels and active state. |
| Side panel/explorer | `ui.panel` | Render supported declarative widgets; do not read the filesystem. |
| Editor panes | `ui.layout`, `view.lines` | Convert logical pane tree to terminal-cell rectangles. |
| Gutter | line metadata/options | Draw absolute or relative line numbers when enabled. |
| Text area | original line bytes and style-role spans | Decode for display, expand tabs, wrap and map bytes to cells. |
| Cursor/selection | `view.cursor`, `view.selection` | Map logical byte offsets to visible cell positions. |
| Statusline | `ui.statusline` | Display Runtime-provided mode/status fields. |
| Prompt | `ui.prompt` | Display the current prompt and send committed text. |
| Notifications | `ui.notify` | Display bounded, dismissible notification state where represented. |

An empty layout, null active leaf or leaf with `active_view: null` is valid
under Protocol 1.0.0. Render an empty-pane state; do not invent a document or
view.

## 3. Adapter architecture

```text
ui/tui/
├── include/cdin/frontend/ui/tui/
└── src/
    ├── input/       # terminal bytes -> raw key/text/paste/pointer events
    ├── platform/    # POSIX termios/poll and Windows console/wait adapters
    ├── model/       # TUI projection and bounded visible-line windows
    ├── layout/      # logical panes -> terminal rows/columns
    ├── text/        # UTF-8, grapheme, width, tab, bidi/cell mapping
    ├── render/      # terminal escape output and frame diffing
    └── session/     # adapter coordination with Core Protocol/session
```

TUI may depend on Frontend Core and its platform adapter only. It must not
include SDL, Backend, Runtime or Kernel headers. It does not depend on a common
GUI renderer or a shared Core text-layout engine.

## 4. Input behavior

The platform input parser consumes arbitrary partial byte reads. It must
preserve incomplete UTF-8 and CSI sequences between reads, distinguish a lone
Escape from the start of an escape sequence using a bounded timeout, and
recognize bracketed paste only when the terminal mode was enabled.

Mapping rules:

- key identity, action and modifiers become `input.key`;
- committed text becomes `input.text`;
- bracketed paste bytes become `input.paste`;
- pointer events become `input.mouse` only when `mouse` was accepted;
- resize and focus are separate Protocol messages.

Do not implement Normal/Insert/Visual mode, `hjkl`, command-line parsing,
search commands, undo/redo bindings or user keymaps in TUI. The Runtime is the
sole authority for those behaviors.

## 5. Visual layout

The TUI owns all cell geometry. It recursively maps the Protocol PaneTree to
terminal rectangles using each split's `ratio_bp`, current terminal size and
local minimum-space policy. The mapping is recomputed on resize and when the
logical tree changes. Visual-line up/down and home/end are calculated from the
cell layout, then converted back to logical byte positions for
`view.set_position`.

When terminal dimensions cannot satisfy all logical panes, the adapter must
use a deterministic local fallback (for example collapse a non-active pane or
show a compact overflow indicator). The exact collapse priority and minimum
pane sizes remain a local implementation decision and must be recorded before
multi-pane release; it must never rewrite the Runtime pane tree.

## 6. Text presentation

TUI renders byte-preserving line data. It owns UTF-8 display decoding,
malformed-byte fallback, grapheme segmentation, terminal display width, tab
expansion, wrapping, selection/cursor mapping and scroll geometry. Combining
marks, variation selectors, ZWJ sequences and wide glyphs must not be treated
as one-byte/one-cell text.

Terminal emulators vary in Arabic shaping and bidi support. The TUI must
maintain byte-to-cell mappings even when the terminal performs final shaping,
and must state its supported Persian/RTL behavior by terminal test matrix.
It must not claim full Persian correctness based on a single terminal.

See [Text presentation](../contracts/text-presentation.md).

## 7. Terminal lifecycle and event loop

On open, confirm the input/output are interactive, save the exact prior
terminal/console state, then apply raw mode, alternate screen, cursor and
bracketed-paste settings required by the adapter. Teardown restores every mode
that was changed.

The application loop must multiplex terminal input, Backend/transport
readiness, resize and termination. It must not block on a whole Protocol frame
while terminal input is pending and must not use sleep-based polling. Signal
handlers set a shutdown flag only; cleanup is performed on the normal loop
thread. Uncatchable termination and power loss are outside the restore
contract.

The TUI now has a transport-agnostic readiness coordinator in
`src/session/event_loop.c`. It requires the `cdin` composition root to provide
a blocking wait callback that multiplexes terminal, Backend, resize and
termination readiness. The Core Protocol client is still blocking and the
platform wait bridge, ESC deadline, signal routing and PTY/console restoration
tests remain unimplemented. See [Event loop and I/O](../contracts/event-loop-and-io.md).

## 8. Capability policy

The TUI advertises only features it can actually implement in its current
terminal session. Likely optional features include `mouse`, `clipboard`,
`truecolor`, `rich_text`, `images`, `dialogs` and `ime`; none may be inferred
from a terminal name alone. `canvas` is not part of the baseline. A request for
an unavailable capability must degrade gracefully or surface a clear
unavailable response.

`units="cells"` is attach metadata, not a capability.

## 9. Current source

Present source includes:

- POSIX raw terminal and Windows console VT-mode adapters;
- terminal open/close lifecycle and alternate-screen handling;
- incremental CSI/UTF-8/Escape/bracketed-paste parser;
- TUI model staging and one active visible line window;
- one-view renderer with line numbers, cursor/selection, status and prompt;
- scroll requests and version-aware line handling;
- bounded readiness-loop coordination with fake wait/drain integration tests.

It does not yet include a standalone TUI executable, platform wait bridge,
readiness-aware Protocol reads, complete layout tree/multiple panes, style-role
and declarative widget rendering, mouse input, full Unicode visual mapping,
concrete transport or live Backend/Runtime E2E. The Headless trace primitive is
tracked separately in [Headless](headless.md).

## 10. Acceptance criteria

A release-ready TUI must satisfy all of the following:

- Protocol 1.0.0 negotiation, complete schema validation and shared conformance
  vectors;
- full semantic snapshot application, including nulls, options, styles,
  panels, 32-view/16-panel bounds and multiple visible line requests;
- readiness-driven event loop with no busy-wait and bounded partial I/O;
- driver/observer enforcement and ACK-after-commit behavior;
- safe startup/teardown on normal exit, errors, EOF, resize and catchable
  signals on Linux and Windows;
- Runtime-provided default commands/keymaps for navigation, insert, delete,
  newline, undo/redo, open/save/quit; TUI contains no duplicate policy;
- byte-correct cursor/selection mapping for supported grapheme/cell cases;
- multi-pane layout and graceful behavior when terminal space is insufficient;
- an honest documented RTL/Persian terminal support matrix;
- live integration test that opens, edits, saves, reopens and byte-compares a
  file through `cdin` → Frontend → Backend → Runtime → Kernel.
