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
document bytes or move selection authority out of Runtime. It tracks up to 32
stable Runtime view IDs, with independent document version, cursor, selection,
scroll anchor, visibility/focus and one outstanding line-window request per
view. A `view.lines` response is accepted only for that view's exact requested
first-line/count/version tuple and remains pending until response-scoped
`ui.flush`. Complete snapshot replacement removes views omitted by the new
PaneTree; a same-version view retains its cache, while a changed version drops
only that view's stale cache. Snapshot revisions must be strictly increasing.

Line bytes and their preserved span-CBOR arrays are held in disposable per-view
windows (up to 64 lines of at most 8192 bytes per window). The aggregate visible
+ pending line-byte and span-CBOR budget is 4 MiB. When a new response would
exceed the budget, the model evicts the least-recently-used completed visible
window, preferring non-focused views; pending responses are never evicted. If
no window can be evicted to satisfy the bound, the response is rejected without
replacing the target window. The renderer traverses the committed PaneTree and
renders every active leaf view in its own clipped rectangle. A
platform-independent model regression exercises interleaved multi-view
responses, span preservation and aggregate LRU eviction even when SDL is
disabled.

## PaneTree geometry foundation

The internal `ui/gui-sdl/src/layout` module maps the bounded semantic PaneTree
to per-node pixel rectangles without depending on SDL or changing the
Protocol/Runtime model. Horizontal splits partition the X axis (first child
left, second child right); vertical splits partition the Y axis (first child
top, second child bottom). The first extent uses integer floor arithmetic on
`ratio_bp`; the second receives the exact remainder, and both children retain
at least one pixel. Unsplittable viewports fail without publishing partial
geometry. The pure geometry tests run even when SDL is disabled.

The geometry engine is now consumed by the committed multi-view model and
multi-pane renderer described above. It is also used by the pure splitter
hit-test API added in [Frontend PR #29](https://github.com/cdin-editor/frontend/pull/29).
That API returns a parsed layout-node index for a configurable pixel hit
radius, including nested divider intersections. Callers resolve the stable
semantic split ID from `layout.nodes[index].id`; the hit-test itself does not
mutate Runtime state or send resize requests. ADR-0024 now accepts the optional `pane-splitter-resize/1` contract.
GUI-SDL implements pointer capture, local preview and the typed release request;
its committed model remains unchanged until the matching result and atomic
snapshot/flush are applied. The capability is gated by
`CDIN_FRONTEND_GUI_SDL_ENABLE_PANE_SPLITTER` (default `OFF`) until CDIN pins
compatible immutable releases and passes the live product matrix in issue
[CDIN #118](https://github.com/cdin-editor/cdin/issues/118).

## Text pipeline design

The production Unicode renderer direction, candidate dependencies, byte/cluster
mapping requirements, cache key and conformance gates are recorded in
[gui-sdl-text-pipeline.md](gui-sdl-text-pipeline.md). The note is an engineering
proposal; it does not add or pin a font/bidi dependency.

## Current rendering and input limits

The SDL renderer now traverses the committed PaneTree and draws each
leaf's active Runtime view in the deterministic pixel rectangle computed by
the SDL-independent geometry module. Each pane has its own clipped drawing
region, split boundary and active/inactive affordance; nested two- and
three-pane layouts are covered by the dummy/software-renderer pixel tests.
Geometry is recomputed from the current pixel viewport on every frame. A
committed empty PaneTree draws no synthetic view, and too-small split geometry
degrades to the focused view rather than dividing into zero-sized panes.

Text is still drawn with SDL's diagnostic debug-text primitive. Non-ASCII
bytes are shown as placeholders; the original bytes remain unchanged in the
cache. The renderer is therefore multi-pane, but not yet a production Unicode
text renderer. Style roles, cursor/selection visualization, panels, widgets
and per-view scrolling remain later phases.

## Semantic presentation payloads and line spans

The GUI model stages the Protocol `ui.style_table`, `ui.panel` and
`ui.prompt` payloads with the same atomic snapshot as its PaneTree and views.
Their CBOR maps are retained byte-for-byte under explicit limits (32 KiB for
the style table, 16 panels at up to 4 KiB each, and 4 KiB for a prompt). A
complete replacement snapshot starts with an empty panel/prompt/style state;
omitted panels disappear, and `prompt:null` clears the prompt. The model does
not interpret arbitrary attribute/widget fields.

Optional `view.lines` spans are validated against the original line byte
length and preserved as their encoded CBOR array beside the line bytes. Span
role IDs and byte offsets are not rewritten. Span payload storage counts
against the same 4 MiB aggregate LRU cache budget as line content. This gives
the later text renderer the original byte-to-role data without prematurely
assigning colors or glyph geometry.

The field-level style attribute and declarative widget schemas remain
under design in [Spec issue #21](https://github.com/cdin-editor/spec/issues/21).
Until that contract is accepted, the GUI does not invent rendering semantics
for opaque panel/prompt maps. Cursor and selection drawing also remains gated
on the byte-to-visual mapping from the text pipeline phase.

## Pane hit-testing and pointer routing

The geometry module also provides SDL-independent point hit-testing against the
same computed pixel rectangles used by the renderer. Rectangles are half-open;
a point exactly on a split boundary belongs to the second child. A successful
hit returns the deepest leaf and its active Runtime View ID. Empty leaves,
removed/stale views, points outside the content viewport and empty layouts
produce no target.

The SDL adapter converts native mouse coordinates with
`SDL_ConvertEventToRenderCoordinates` before hit-testing, so high-DPI scaling,
renderer scaling and the viewport use the same drawable-pixel coordinate
space. On mouse press and wheel events it sends the hit leaf's **active Runtime
View ID** as `input.mouse.target`; it never sends an internal Split/Leaf node
ID. Runtime remains the authority that accepts the target and changes
semantic focus. Mouse motion and button release do not trigger a focus change.
The renderer viewport and startup dimensions are initialized from drawable
pixel dimensions, not logical window dimensions.

The current Runtime accepts `input.mouse.target` as a View ID, while the
Protocol architecture text describes it generically as a region ID. The exact
splitter/region identity and ratio-drag operation are not yet standardized.
Split resize is therefore gated by [Spec issue #20](https://github.com/cdin-editor/spec/issues/20);
Frontend does not invent a new mouse action or mutate Runtime-owned ratios
locally.

The adapter advertises the mouse capability and forwards negotiated pointer
and wheel input. It advertises `pane-splitter-resize/1` only when the explicit
build option is enabled; normal package builds keep that option off. The
accepted `ime-composition/1` capability is also opt-in through
`CDIN_FRONTEND_GUI_SDL_ENABLE_IME_COMPOSITION`; normal package builds keep it
off until CDIN's live IME matrix passes. Dialogs, clipboard-provider, images,
canvas and rich-text remain unadvertised. Full grapheme mapping, fallback
fonts, shaping, bidi and Persian/Arabic rendering remain open. Selection
messages are currently not cached or rendered; Runtime remains the owner of
selection semantics.

The proposed FreeType/HarfBuzz/FriBidi stack is not a finalized contract and
is not silently added as a dependency. SDL3 itself remains an optional build
dependency selected with `CDIN_FRONTEND_BUILD_UI_SDL=ON`.


## Borrowed Session lifetime

The Composition Root owns the Core Session and its transport. GUI-SDL borrows
that Session and never detaches or destroys it.

When the borrowed Session leaves `ATTACHED` (including `DETACH_SENT`), the
adapter fails closed:
- incoming semantic messages are rejected before they can mutate the staged
  or committed presentation model, and a stale `ui.flush` is never ACKed;
- rendering cached content and issuing additional line-window requests are
  rejected;
- queued input and renderer events are ignored, while native quit/window-close
  events remain available so the host can exit;
- `destroy()` releases only GUI-owned SDL/model resources; the Composition
  Root remains responsible for Session detach, transport shutdown and Session
  destruction.

The SDL adapter smoke now exercises a detached inbound flush, verifies that
no ACK or committed state transition occurs, checks that cached render is
refused, confirms queued text input is dropped, preserves the quit event, and
runs repeated create/render/destroy cycles after forced partial-startup
failure. This is a component lifecycle sub-gate, not proof of transport
reconnect or Backend/Runtime process recovery.


## Current verification record

The SDL3-specific workflow uses SDL3 3.4.16 built from a checksum-pinned
archive and the production logger. It validates partial-startup rollback,
adapter creation/render, SDL event delivery, Protocol mouse forwarding, the
pure geometry/splitter hit-test suites, and borrowed-Session detach behavior.
It runs GUI-SDL tests under AddressSanitizer, installs the static library and
verifies an independent CMake consumer can find and link
`cdin::frontend_ui_gui_sdl`. Frontend CI run
[42446596171](https://github.com/cdin-editor/frontend/actions/runs/42446596171)
passed all five jobs: Linux, macOS, Windows, SDL3/ASan + installed consumer,
and Pipeline quality.

This is component-level verification. It does not validate CDIN's transport,
Backend/Runtime startup, a complete editing session, reconnect, installer
behavior or release-grade Unicode text rendering. Keep the GUI-SDL adapter
opt-in until the CDIN live-session smoke and the remaining rendering/recovery
gates pass.

## Negotiated native IME composition

GUI-SDL supports the accepted optional `ime-composition/1` lifecycle when
built with `CDIN_FRONTEND_GUI_SDL_ENABLE_IME_COMPOSITION=ON` and the attached
Core Session confirms that capability in `hello_ack.accepted_capabilities`.
The option defaults to **OFF**; ordinary package and product builds do not
advertise it until the Composition Root pins compatible component candidates
and the live CDIN IME matrix passes.

Composition IDs use the Core Client's existing monotonic outbound
Protocol sequence, scoped to that connection rather than to a GUI window or
Session object. The ID is sampled immediately before `ime.composition.start`,
whose envelope consumes the same sequence. This avoids adding composition
allocator state to the public Session struct ABI. Recreating the GUI adapter
while the same Client remains attached therefore cannot reuse an ID already
sent on that connection; an exhausted sequence returns zero instead of
wrapping.

SDL 3.2+ `SDL_EVENT_TEXT_EDITING` is handled as transient preedit. SDL reports
the start and selected length in UTF-8 character positions; Core Session
converts those positions to UTF-8 byte offsets at code-point boundaries before
sending `ime.composition.update`. Preedit bytes, caret and selection remain
inside the originating GUI adapter. They are not added to the semantic model,
document cache, Runtime, Kernel, `ui.flush` snapshots or Observer sessions.
The initial debug renderer shows one placeholder per preedit code point and
marks its local selected range/caret; production font shaping, grapheme-aware
visual placement, bidi and Persian/Arabic rendering remain separate Frontend
#15 gates.

On the matching SDL committed-text event, GUI-SDL sends one
`core/input.text` with `composition_id`; ordinary committed text remains
unchanged when the optional capability was not accepted. Native cancel,
Escape, focus loss, hidden-window transitions, view/document changes, Driver
loss and Backend-forced cancellation clear the matching local composition.
An empty preedit update is valid; an empty final string cancels rather than
creating a document edit. Detach discards transient state and never resumes
it after reattach.

The Core Protocol codec validates the optional namespace, non-zero IDs,
the 4096-byte preedit bound, UTF-8 validity and byte-boundary offsets. Core
Session allocates strictly increasing IDs for the attached client connection
and enforces capability acceptance, Driver role and the
Frontend/Backend-specific cancellation reason sets. The Backend remains
responsible for per-connection lifecycle state, replay rejection and atomic
commit admission; Runtime remains responsible for revalidating the captured
view/version/selection before applying committed text. The GUI-SDL capability
remains opt-in until CDIN #118 proves the exact immutable
Frontend/Backend/Runtime stack end-to-end.
