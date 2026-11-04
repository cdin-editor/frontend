# Text presentation and visual-position mapping

The Kernel and Frontend Protocol use logical byte offsets. The Frontend turns
version-pinned line bytes into cells or pixels. This document describes the
shared invariants and the adapter-owned algorithms.

## 1. The byte-preservation rule

Document content is an arbitrary byte sequence. It may include NUL, BOM,
invalid UTF-8 and mixed/explicit line endings. `view.lines` returns original
bytes. The Frontend may decode or substitute characters for display, but it
must never rewrite, normalize, drop or transcode the source bytes in a way that
is sent back as document content.

Rendering invalid data is a display decision. Possible display treatments
include U+FFFD replacement, escaped byte notation or a replacement glyph. The
chosen treatment must be reversible enough for navigation and must retain the
original byte mapping. It is not a document mutation.

## 2. Coordinate spaces

Keep these coordinate spaces separate:

| Space | Owner | Purpose |
| --- | --- | --- |
| Document byte offset | Kernel/Protocol | Canonical cursor and selection positions. |
| UTF-8 code point boundary | Adapter text pipeline | Safe decoding and byte-to-code-point mapping. |
| Grapheme cluster boundary | Adapter text pipeline | User-perceived character navigation/render grouping. |
| Visual run/glyph position | Adapter text pipeline | Shaping and bidi ordering. |
| Terminal cell | TUI | Cursor/draw placement in character cells. |
| Pixel position | GUI-SDL | Cursor/draw placement in window geometry. |

Only byte positions cross the Frontend Protocol boundary for document cursor,
selection and `view.set_position`. The TUI and GUI calculate visual movement
locally and map the result back to a byte offset at the same document version.

## 3. Adapter-owned text pipeline

Each adapter owns its implementation of:

1. line-byte segmentation and UTF-8 decoding for display;
2. malformed UTF-8 fallback while preserving original byte ranges;
3. grapheme cluster boundaries and visual cursor stop positions;
4. tab expansion using the current `tab_width` option and visual column;
5. Unicode display width / terminal cell or glyph metrics;
6. shaping and font fallback where the environment requires it;
7. bidirectional run ordering and mapping between logical and visual order;
8. wrapping, clipping and scroll-to-cursor behavior;
9. reverse mapping from visual actions to logical byte offsets.

No shared `core/text` rendering or layout engine is mandated. A future library
may be consumed by more than one adapter only after an explicit boundary
review; it must not make GUI and TUI share a rendering pipeline by accident.

## 4. TUI considerations

A terminal typically performs final glyph rasterization and may perform Arabic
shaping/bidi, but this behavior varies across terminal emulators and fonts. The
TUI must not assume that all terminals display Persian/Arabic and mixed RTL/LTR
text identically.

The TUI must maintain a mapping from each displayed grapheme/cell range back
to its original byte range. Wide characters may occupy two cells; combining
marks may occupy zero additional cells; tabs occupy a context-dependent number
of cells. Cursor placement and selection painting must use this mapping rather
than treating UTF-8 byte count as terminal width.

Where the terminal does not provide predictable bidi behavior, the TUI must
report the supported behavior honestly and retain byte-correct navigation. A
complete portable Persian TUI guarantee is not made by the current source.

## 5. GUI-SDL considerations

The desktop adapter owns its font and shaping path. A proposed native stack is
SDL3 for window/input/render integration, FreeType for font face access,
HarfBuzz for shaping and FriBidi for bidirectional ordering. This is an
implementation proposal, not a pinned dependency decision: exact versions,
font fallback policy, glyph-cache policy, and licensing/build matrix need a
separate design review before code and release metadata are frozen.

The GUI must map shaped glyph clusters back to original byte ranges so cursor,
selection, hit-testing and diagnostics retain logical positions. Pixel layout
and glyph atlases remain entirely under `ui/gui-sdl/`.

## 6. Versioning and invalidation

Derived line layout depends on more than document version. It can also depend
on viewport width, tab width, wrap option, font, scale, theme/style table,
terminal width table and Unicode data version. The adapter must invalidate
only the affected presentation cache when any dependency changes; it must
never mutate or re-version the source document to achieve a visual result.

## 7. Conformance corpus

The shared corpus must include:

- ASCII, multibyte UTF-8 and multi-code-point grapheme clusters;
- combining marks, variation selectors and emoji ZWJ sequences;
- East Asian wide and ambiguous-width characters;
- tabs at different visual columns;
- valid Persian/Arabic runs, mixed RTL/LTR, punctuation and numbers;
- invalid UTF-8, embedded NUL, BOM, CR, LF and CRLF;
- cursor and selection at line boundaries and at valid byte boundaries;
- wrapping and resize changes that preserve logical cursor/selection bytes.

The expected result is not one identical image across adapters. Tests assert
byte-to-visual mapping invariants, grapheme boundaries, cursor validity and
preservation of source bytes. Adapter-specific golden images/cell frames are
separate from shared coordinate conformance.
