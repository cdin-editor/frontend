# GUI-SDL UI peer

The reusable production foundation is exported as
`cdin::frontend_ui_gui_sdl`; the standalone `cdin_frontend_gui_sdl_preview`
remains a separate development app.

The Composition Root owns the transport and Core Session. It negotiates and
attaches the Session, then passes the attached Session to the GUI adapter.
The GUI peer owns SDL window/renderer lifetime, native keyboard and committed
text input, pixel resize/focus forwarding, a view-specific semantic model and
a bounded version-pinned visible-line cache. It does not link Backend,
Runtime, Kernel or the TUI adapter.

The current renderer is deliberately an ASCII/debug-text baseline. It does
not claim production font fallback, grapheme shaping, bidi, IME composition,
mouse capabilities, pane-tree rendering, panels or plugin canvas support.
The GUI does not advertise those capabilities. Further implementation and
native SDL CI remain release gates.

Build the preview (which also builds the production adapter library) with
`make build-gui`; SDL3 3.2+ is required. Applications consuming the library
enable `CDIN_FRONTEND_BUILD_UI_SDL=ON` and link
`cdin::frontend_ui_gui_sdl`. See [GUI-SDL architecture](../../docs/frontends/gui-sdl.md).
