# UTF-8 scalar decoder

This module is part of the independent GUI-SDL adapter. It is deliberately
UI-local and has no SDL or Frontend Core dependency.

The decoder reads length-delimited document bytes and returns one Unicode
scalar with its exact half-open source byte range. It performs no allocation,
never modifies the input, and treats embedded NUL as valid U+0000.

Malformed and truncated UTF-8 is represented as U+FFFD with
`valid_utf8 == 0`; exactly one original byte is consumed for each replacement
result. This makes every returned range reversible to the original document
bytes, including invalid sequences.

This is only the decoding layer. It does **not** implement grapheme segmentation,
font fallback, shaping, bidi reordering, wrapping, glyph hit-testing or
rendering. Those remain separate Phase 6 work and require an explicit,
version-pinned text-stack decision.
