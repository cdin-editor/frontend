# GUI-SDL — native desktop frontend

**Spec status:** accepted peer frontend. **Implementation status:** an
installable/exported `cdin::frontend_ui_gui_sdl` library now exists, with
Core Session event forwarding and an independent bounded, version-pinned
visible-line cache. The current renderer is an ASCII/debug-text baseline;
it is not a complete desktop editor.

## Ownership and boundary

GUI-SDL is a peer of TUI and Headless. It consumes the public Core Session and
Protocol APIs and owns SDL window, renderer, event translation, pixel
resize/focus and GUI presentation state. It does not include TUI headers,
link Backend, Runtime or Kernel, own document bytes, or interpret editor
keymaps.

The Composition Root owns transport readiness, Core Client/Session lifetime,
HELLO/HELLO_ACK, ATTACH and Protocol message delivery. GUI-SDL requires an
already attached Driver Session. It forwards physical non-text key events,
committed UTF-8 text, clipboard paste, focus and pixel resize through Core
Session; ordinary printable keydowns are not duplicated as both key and text.

## Model and cache

The GUI has a separate model implementation. It checks view identity and
document versions, stages line responses until response-scoped `ui.flush`,
acknowledges committed flushes, and requests version-pinned visible ranges
after the snapshot ACK. Its fixed cache is bounded to 64 lines of at most
8192 bytes each. The renderer never mutates cached document bytes. A
platform-independent model regression runs even when SDL is disabled.

## Current rendering and input limits

The first renderer uses SDL's diagnostic debug-text primitive and a simple
pixel viewport. Non-ASCII bytes are shown as placeholders; the original bytes
remain unchanged in the cache. It currently presents one attached view rather
than rendering the full PaneTree. Style roles, complete options/layout
projection, selection/cursor visualization, panels, widgets, scrolling and
device-loss recovery are not yet complete.

The adapter does not advertise mouse, IME, dialogs, clipboard-provider,
images, canvas or rich-text capabilities. It only forwards key, committed
text, paste, focus and resize through the Core Session contract. SDL text
composition/IME, full grapheme mapping, fallback fonts, shaping, bidi and
Persian/Arabic rendering remain open.

The proposed FreeType/HarfBuzz/FriBidi stack is not a finalized contract and
is not silently added as a dependency. SDL3 itself remains an optional build
dependency selected with `CDIN_FRONTEND_BUILD_UI_SDL=ON`.
