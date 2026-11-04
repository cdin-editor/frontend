# GUI-SDL UI peer

SDL3 is confined to this peer. The current `app/main.c` is a runnable desktop
smoke preview built by the `cdin_frontend_gui_sdl_preview` target; it is not the
production editor adapter and does not connect to Runtime/Backend.

The planned production library remains `cdin::frontend_ui_gui_sdl`. Its
independent modules are `input/`, `platform/`, `layout/`, `text/`, `render/` and
`session/`. It must consume Core Protocol/session/model APIs, own pixel layout
and text presentation, and must not include TUI headers or Backend, Runtime or
Kernel implementation headers.

Build and launch the current preview with `make build-gui` and `make run-gui`
(SDL3 3.2+ is required). See [GUI-SDL architecture](../../docs/frontends/gui-sdl.md)
for the production implementation and acceptance gates.
