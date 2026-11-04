# GUI-SDL — native desktop frontend

**Spec status:** accepted peer frontend; SDL ownership is constrained to this
adapter by FE-020 and FE-023. **Implementation status:** an isolated SDL3
smoke-preview executable exists in `ui/gui-sdl/app/main.c`; it is not yet the
production GUI adapter or a live editor.

## 1. Purpose and boundary

`ui/gui-sdl` is the native desktop window implementation of the Frontend
Protocol. It is a peer of `ui/tui` and `ui/headless`, not a renderer shared with
TUI. It consumes Core Protocol/session/model APIs and owns all pixels, window
resources, font machinery, shaping, bidi, pointer handling and GUI drawing.

It must not link Backend/Runtime/Kernel, call Runtime methods, own document
bytes or interpret keymaps. All SDL headers and calls stay inside this
subdirectory (FE-020).

## 2. Module structure

```text
ui/gui-sdl/
├── include/cdin/frontend/ui/gui_sdl/
└── src/
    ├── input/       # SDL keyboard, text/IME, mouse, wheel, focus, resize
    ├── platform/    # SDL init, window, renderer/device lifecycle, clipboard
    ├── layout/      # logical pane tree -> pixel rectangles and visual rows
    ├── text/        # UTF-8, grapheme mapping, fallback, shaping, bidi
    ├── render/      # draw batches, glyph atlas, clipping, panels and overlays
    └── session/     # GUI-specific orchestration around Core session/model
```

The preview executable target is `cdin_frontend_gui_sdl_preview`. The planned
production library target is `cdin::frontend_ui_gui_sdl`; it may link Core,
SDL3 and its explicitly approved text/font dependencies only. It must not link
TUI. The preview currently links only Core and SDL3.

## 3. Window and frame lifecycle

1. Initialize SDL subsystems required by the adapter.
2. Create the window and rendering resources; retain exact ownership of every
   created resource.
3. Determine actual window/input capabilities and prepare the frontend
   capability request.
4. Connect through the composition-root transport and perform Protocol
   negotiation/attach.
5. Consume complete committed snapshots; request visible version-pinned lines.
6. Map the logical pane tree to pixel geometry and derive text/glyph layout.
7. Render a frame only when model, geometry, text/font state or presentation
   options are dirty.
8. On shutdown, detach when possible, stop callbacks/workers, release glyph
   and render resources, destroy the window and quit only the SDL subsystems
   owned by this adapter.

The adapter must not let a renderer callback outlive the SDL resources it uses.
All resource ownership and failure cleanup paths need tests.

## 4. Visual composition

Use the same semantic content as TUI but express it with desktop geometry:

- optional title/header or tabline;
- editor panes from the Runtime logical layout tree;
- gutter and version-pinned line content;
- style-role rendering and selections;
- optional declarative panels, prompts, status and notifications;
- capability-backed clipboard/dialog requests;
- optional plugin canvas only when `canvas` is accepted.

A null `active_view` is an empty pane, not a frontend error. A null root is an
empty layout. The GUI does not synthesize documents or commands to fill space.

## 5. Input mapping

| SDL/device event | Protocol behavior |
| --- | --- |
| Physical key down/up/repeat | `input.key` with stable key identity/action/modifiers |
| Committed text / IME result | `input.text` |
| Clipboard paste bytes | `input.paste` |
| Pointer/button/wheel | `input.mouse`, only if `mouse` accepted |
| Window dimensions | `window.resize` with `units="pixels"` |
| Focus | `focus` |

The GUI must not turn a key combination into a Runtime command. It emits
physical input only. Text composition events are not collapsed into keycodes.

## 6. Text pipeline and proposed dependency stack

The GUI owns a desktop text pipeline that maps logical byte ranges to glyph
clusters and pixel positions. It must support font fallback, grapheme
boundaries, shaped runs, bidi order, wrapping, hit testing, selection and
cursor mapping.

A candidate native stack is:

- **SDL3** for window, event and render-device integration;
- **FreeType** for font-face access and glyph metrics;
- **HarfBuzz** for shaping;
- **FriBidi** for Unicode bidirectional ordering.

This stack is a proposal, not a finalized dependency contract. Before
implementation, settle the exact versions, CMake package strategy, fallback
order, scale/DPI behavior, font-cache memory budget, build/license obligations
and supported OS matrix. Keep this work isolated under `ui/gui-sdl/`.

The text path must retain byte-to-grapheme and grapheme-to-glyph cluster maps.
A shaped glyph index must never replace a logical document byte position.
Persian/Arabic support requires real mixed-direction, punctuation, combining
mark and fallback-font tests, not only screenshot review.

## 7. Capabilities

Advertise only services actually available in the current window/session.
Potential desktop capabilities are `clipboard`, `dialogs`, `mouse`,
`hit_regions`, `rich_text`, `images`, `truecolor`, `ime` and optionally
`canvas`. `units="pixels"` is attach metadata, not a capability. An accepted
capability is per connection and must be checked before sending/servicing its
messages.

## 8. Implementation stages

1. SDL3 lifecycle, window, event wait and clean teardown.
2. Core Protocol 1.0.0 negotiation and semantic snapshot model.
3. Pixel pane layout and visible-line request lifecycle.
4. Input key/text/IME/paste/resize/focus mapping.
5. Text shaping, font fallback, grapheme and bidi mapping.
6. Styles, cursor, selection, panels, widgets and notifications.
7. Clipboard/dialog/mouse/hit-region capability implementations.
8. Cross-platform CI, stress/fault tests and live Backend/Runtime E2E.

Build the current smoke preview with `make build-gui` and launch it with
`make run-gui` (SDL3 3.2+ and the production logger dependency are required;
`LOGGER=stub` is available for offline smoke testing only). The preview uses
SDL's debug text path, reports diagnostics through the Frontend-owned logger,
and advertises no Protocol capabilities. It does not implement a live session,
font fallback, shaping, bidi, semantic widgets or document editing. Do not treat
the preview as satisfying the production GUI release gate.
