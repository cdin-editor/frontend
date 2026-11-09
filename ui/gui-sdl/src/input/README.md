# GUI-SDL input module

Pointer capture and local-only splitter preview are implemented by the SDL
adapter in `../gui_sdl.c`; the shared geometry/hit-test API remains SDL-free.
The input handling must never mutate the committed PaneTree while dragging or
send a protocol message on pointer motion. Only a matching pointer release may
send one typed `pane.splitter.resize` request.
