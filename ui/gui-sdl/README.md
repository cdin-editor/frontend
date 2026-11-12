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

The renderer still uses SDL's diagnostic debug-text primitive and displays
one placeholder per non-ASCII Unicode scalar; it does not provide shaping,
font fallback, grapheme layout or bidi. It now validates the accepted
ADR-0026 style-role table and applies RGBA foreground/background with
source-over alpha compositing plus supported debug-font emphasis to
byte-anchored line spans. Missing colors inherit the pane base; the role
background is composited over that base before the foreground is composited
over the result. This is an incremental presentation step, not a production
Unicode renderer. It does not yet render
declarative panels/widgets, cursor/selection or plugin canvas content. Mouse input is advertised when implemented;
the optional splitter capability is advertised only when the explicit
`CDIN_FRONTEND_GUI_SDL_ENABLE_PANE_SPLITTER` build option is enabled and
the peer accepts the capability.

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
