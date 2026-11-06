# GUI-SDL — native desktop frontend

**Spec status:** accepted peer frontend. **Implementation status:** the
installable/exported `cdin::frontend_ui_gui_sdl` foundation, bounded semantic
model and Core Session input adapter pass the latest native and SDL-specific
CI matrix. [Run 42295241142](https://github.com/cdin-editor/frontend/actions/runs/42295241142)
passed Linux/macOS/Windows and Pipeline quality, the GUI model/adapter tests
under AddressSanitizer, installation and a clean-prefix consumer. CDIN's live
Backend/Runtime composition smoke remains a separate gate tracked in
[CDIN PR #110](https://github.com/cdin-editor/cdin/pull/110). The renderer is
still an ASCII/debug-text baseline; this is not a complete desktop editor.

## Ownership and boundary

GUI-SDL is a peer of TUI and Headless. It consumes the public Core Session and
Protocol APIs and owns SDL window, renderer, event translation, pixel
resize/focus and GUI presentation state. It does not include TUI headers,
link Backend, Runtime or Kernel, own document bytes, or interpret editor
keymaps.

The Composition Root owns transport readiness, Core Client/Session lifetime,
HELLO/HELLO_ACK, ATTACH and Protocol message delivery. GUI-SDL requires an
already attached Driver Session whose attach units are `pixels`; cell-based
sessions are rejected rather than silently interpreting pixel dimensions as
cells. It forwards physical non-text key events, committed UTF-8 text, clipboard
paste, negotiated pointer/wheel input, focus and pixel resize through Core
Session. SDL events addressed to another window are ignored, and ordinary
printable keydowns are not duplicated as both key and text.

## Model and cache

The GUI has a separate presentation model; it does not cache canonical
document or selection state. It checks Runtime view identity and document
versions, accepts line data only when it matches the currently outstanding
`first_line`/count/version request, stages that response until response-scoped
`ui.flush`, and requests version-pinned visible ranges only after snapshot ACK.
Snapshot revisions must be strictly increasing. Its fixed cache is bounded to
64 lines of at most 8192 bytes each. The renderer never mutates cached document
bytes. A platform-independent model regression runs even when SDL is disabled.

## Current rendering and input limits

The first renderer uses SDL's diagnostic debug-text primitive and a simple
pixel viewport. Non-ASCII bytes are shown as placeholders; the original bytes
remain unchanged in the cache. It currently presents one attached view rather
than rendering the full PaneTree. Style roles, complete options/layout
projection, selection/cursor visualization, panels, widgets, scrolling and
device-loss recovery are not yet complete.

The adapter advertises the mouse capability and forwards negotiated pointer
and wheel input. It does not advertise IME, dialogs, clipboard-provider,
images, canvas or rich-text capabilities. SDL text composition/IME, full
grapheme mapping, fallback fonts, shaping, bidi and Persian/Arabic rendering
remain open. Selection messages are currently not cached or rendered; Runtime
remains the owner of selection semantics.

The proposed FreeType/HarfBuzz/FriBidi stack is not a finalized contract and
is not silently added as a dependency. SDL3 itself remains an optional build
dependency selected with `CDIN_FRONTEND_BUILD_UI_SDL=ON`.


## Current verification record

The SDL3-specific workflow uses SDL3 3.4.16 built from a checksum-pinned
archive and the production logger. It validates partial-startup rollback,
adapter creation/render, SDL event delivery and Protocol mouse forwarding under
AddressSanitizer, then installs the static library and verifies an independent
CMake consumer can find and link `cdin::frontend_ui_gui_sdl`. The complete
Frontend CI run 42295241142 passed these checks together with the Linux,
macOS, Windows and Pipeline matrix.

This is component-level verification. It does not validate CDIN's transport,
Backend/Runtime startup, a complete editing session, reconnect, installer
behavior or release-grade Unicode text rendering. Keep the GUI-SDL adapter
opt-in until the CDIN live-session smoke and the remaining rendering/recovery
gates pass.
