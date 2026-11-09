# GUI-SDL UI peer

The reusable production foundation is exported as
`cdin::frontend_ui_gui_sdl`; the standalone `cdin_frontend_gui_sdl_preview`
remains a separate development app.

The Composition Root owns the transport and Core Session. It negotiates and
attaches the Session, then passes the attached Session to the GUI adapter.
The GUI peer owns SDL window/renderer lifetime, native keyboard, committed
text and negotiated pointer/wheel input, pixel resize/focus forwarding, a
view-specific presentation model and a bounded version-pinned visible-line
cache. Its attached Core Session must use `units="pixels"`; cell-based sessions
are rejected. Responses are correlated with the requested visible-line window,
and SDL input addressed to other windows is ignored. The GUI does not retain
Runtime-owned selection state. It does not link Backend, Runtime, Kernel or the
TUI adapter.

The current renderer is deliberately an ASCII/debug-text baseline. It does
not claim production font fallback, grapheme shaping, bidi, IME composition,
pane-tree rendering, panels or plugin canvas support. The GUI advertises its
implemented mouse capability only; further rendering, recovery and native SDL
CI remain release gates.

Build the preview (which also builds the production adapter library) with
`make build-gui`; SDL3 3.2+ is required. Applications must include `cdin_frontend_gui_sdl_supported_capabilities()` in
the Core Session capability mask before HELLO to enable pointer input. The SDL
adapter smoke runs with the dummy video driver and verifies partial-startup
rollback, pixel-unit validation, foreign-window event isolation and
`input.mouse` forwarding. Applications consuming the library
enable `CDIN_FRONTEND_BUILD_UI_SDL=ON` and link
`cdin::frontend_ui_gui_sdl`. See [GUI-SDL architecture](../../docs/frontends/gui-sdl.md).

## PaneTree splitter resize

GUI-SDL implements pointer capture, pixel-only preview and one typed
`pane.splitter.resize` commit request per release. It does not update the
committed model locally: the preview is retained until the matching
`pane.splitter.result` and subsequent atomic `ui.layout`/`ui.flush` commit.
Escape, focus loss, detach, Driver loss and a changed committed layout
revision discard the preview. The capability request is deliberately gated:
`CDIN_FRONTEND_GUI_SDL_ENABLE_PANE_SPLITTER` defaults to `OFF`. CDIN must enable
it only after the compatible Backend/Runtime/Frontend releases are pinned and
the live product matrix in CDIN issue #118 passes.
