# Text presentation primitives

This directory belongs to the independent GUI-SDL adapter. It is not part of
Frontend Core and does not own document bytes.

## Implemented

- `utf8.c` strictly decodes UTF-8 byte windows into Unicode scalar records.
- Every record retains the original source byte offset and length.
- Invalid source bytes map to U+FFFD one byte at a time for display; source bytes
  are never rewritten.
- Byte-offset lookup accepts scalar boundaries and rejects offsets inside a
  valid multibyte sequence.
- The decoder is allocation-free and can be built/tested without SDL.

The decoder is deliberately not a grapheme segmenter. A combining sequence and
an emoji ZWJ sequence still produce multiple scalar records. Higher layers
must map these scalar ranges into grapheme boundaries and shaped glyph clusters.

## Still required for production text

- Pin and integrate a font rasterization and shaping stack.
- Implement Unicode grapheme segmentation against the release-pinned Unicode
  data version.
- Implement paragraph bidi resolution, script/language runs and font fallback.
- Shape, wrap and map logical byte ranges to visual glyph clusters.
- Render glyph runs and use the mapping for cursor, selection and hit-testing.

No external text-rendering dependency is currently selected or linked. See the
GUI-SDL text pipeline design note in `docs/frontends/gui-sdl.md`.
