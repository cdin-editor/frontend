# GUI-SDL production text pipeline

**Status:** Engineering proposal. This document records a candidate architecture and
acceptance plan; it does not select or add a production dependency. The candidate
must pass the native package matrix and clean-consumer test before the dependency
choice is finalized.

Related: [Frontend #15, Phases 5–7](https://github.com/cdin-editor/frontend/issues/15),
[Spec #14, IME contract](https://github.com/cdin-editor/spec/issues/14),
[Spec #21, presentation schemas](https://github.com/cdin-editor/spec/issues/21).

## 1. Current boundary

The Protocol and Runtime use logical document byte offsets. GUI-SDL owns every
visual transform and cache: UTF-8 decoding for display, grapheme segmentation,
bidi, shaping, fallback, wrapping, line geometry, hit-testing and glyph drawing.
None of these operations may rewrite document bytes or send visual coordinates
back as logical positions.

The new `ui/gui-sdl/src/text/utf8.c` primitive is the first building block. It
maps each source byte range to a Unicode scalar and replacement display value
when necessary. It intentionally does not merge combining marks, perform
grapheme segmentation, shape text or reorder paragraphs.

The legacy Lua `data/core/text/bidi.lua` and `shaper.lua` are useful historical
references, not a production implementation: bidi classification is range-based
and reverses characters inside heuristic runs; Arabic shaping is a hand-written
subset of presentation forms. GUI-SDL must not port those approximations as its
Unicode conformance implementation.

## 2. Candidate dependency direction

The candidate is **SDL3_ttf + HarfBuzz/FreeType + FriBidi**, with Unicode
grapheme and line-break data pinned separately for GUI-SDL.

### SDL3_ttf, HarfBuzz and FreeType

SDL3_ttf is the SDL-native font subsystem candidate. Its upstream project wraps
FreeType and HarfBuzz and is distributed under the zlib license; the upstream
README also identifies the licenses of its font/vector dependencies:
[SDL3_ttf README](https://github.com/libsdl-org/SDL_ttf#readme).

The SDL3_ttf text API offers:

- renderer text engines and persistent text objects;
- explicit text direction and script selection when built with HarfBuzz;
- fallback font registration;
- text-to-substring queries by byte offset and point;
- substring records with byte offset, byte length, line index, cluster index
  and rendered rectangle.

See the [SDL3_ttf API index](https://wiki.libsdl.org/SDL3_ttf/index),
[TTF_SetTextDirection](https://wiki.libsdl.org/SDL3_ttf/TTF_SetTextDirection),
[TTF_SetTextScript](https://wiki.libsdl.org/SDL3_ttf/TTF_SetTextScript),
[TTF_GetTextSubStringForPoint](https://wiki.libsdl.org/SDL3_ttf/TTF_GetTextSubStringForPoint)
and [TTF_SubString](https://wiki.libsdl.org/SDL3_ttf/TTF_SubString).

The build must explicitly require HarfBuzz support; an SDL3_ttf build without
HarfBuzz cannot satisfy the RTL shaping requirement. The version, source archive,
checksum, static/shared policy, transitive license notices and installed CMake
target must be pinned and tested. No system-installed, unversioned TTF library
may silently substitute for the tested build.

HarfBuzz provides OpenType shaping for Arabic and other complex scripts and
supports emoji/ZWJ sequences. Its shaping output is not by itself paragraph
bidi resolution. See the [HarfBuzz shaping overview](https://harfbuzz.github.io/what-does-harfbuzz-do.html).

### FriBidi for paragraph ordering

The candidate bidi engine is GNU FriBidi, which implements the Unicode
Bidirectional Algorithm (UAX #9) and has no library dependencies:
[GNU FriBidi](https://github.com/fribidi/fribidi).

The frontend must resolve paragraph embedding levels and visual run order
before shaping directional runs. It must not reverse UTF-8 bytes or codepoints
to simulate RTL. Each shaped visual run retains a map to its original logical
byte range. License handling must include the selected LGPL version and
Unicode-data notices in the source and binary distribution.

### Unicode segmentation and line breaking

Grapheme segmentation is a separate layer from shaping. The implementation
must pin a Unicode release and consume the matching
`GraphemeBreakProperty.txt`, `emoji-data.txt` and
`GraphemeBreakTest.txt` data. The selected version and generated-table hash
must be visible in build metadata and release notes.

Line wrapping must work in logical text order and preserve original byte
ranges. A line break may never split a grapheme cluster. Tabs are expanded in
visual layout only; the source byte remains a tab.

The Unicode release must be documented independently from the font/shaper
library versions. A dependency update that changes cluster boundaries or line
breaks is a visible frontend compatibility change and must run the full
conformance suite.

## 3. Pipeline and mapping contract

The target pipeline is:

1. **Source window:** receive a version-pinned line byte string and the
   corresponding optional semantic byte spans. Retain both unchanged.
2. **Decode:** use the strict UTF-8 decoder. Valid scalars keep exact byte
   ranges; malformed bytes display as U+FFFD with one-byte source mapping.
3. **Grapheme segmentation:** compute extended grapheme clusters from the
   pinned Unicode tables. Cursor movement and selection endpoints snap only
   to the defined logical cluster boundaries; protocol positions remain bytes.
4. **Bidi:** resolve paragraphs with UAX #9 and produce directional/script runs
   plus embedding levels. Keep logical indices alongside the visual run list.
5. **Font fallback and shaping:** choose fonts for whole clusters, shape each
   script/direction run with HarfBuzz, and preserve the shaper's cluster values
   back to original UTF-8 byte ranges.
6. **Line layout:** wrap at valid line-break opportunities, expand tabs using
   the current tab width, and place visual runs without changing source bytes.
7. **Paint and hit-test:** draw glyph runs using SDL3_ttf/SDL renderer support.
   Point hit-testing returns a logical byte boundary through the cluster map.
8. **Cursor and selection:** project Runtime byte offsets to cluster edges;
   draw selection rectangles across every affected visual run, including
   mixed-direction lines.

A byte offset inside a grapheme is not silently reinterpreted as a codepoint
index. The layout must have an explicit nearest legal visual caret policy for
such positions while preserving the exact Runtime byte offset in the semantic
model.

## 4. Cache model

Text layout is disposable and keyed by all inputs that can affect geometry:

- view ID and document version;
- source line identity/bytes and style spans;
- viewport width/height and drawable scale;
- font identity, size, fallback chain and font generation;
- wrap and tab-width options;
- paragraph direction, script/language and pinned Unicode data version.

A document-version change invalidates the affected line layout. A viewport,
font or scale change invalidates visual geometry but not document bytes.
Eviction must remain inside the existing 4 MiB aggregate line/span budget plus
a separately declared glyph/text-atlas budget. No unbounded per-view text cache
is permitted.

## 5. Conformance suite

The renderer is not production-ready until these fixtures pass on every
supported platform:

- Unicode GraphemeBreakTest for the pinned Unicode release;
- Unicode BidiTest and BidiCharacterTest for the pinned data version;
- valid and malformed UTF-8, including overlongs, surrogates, truncated
  sequences and offsets at/inside multibyte sequences;
- Persian and Arabic joining with marks, presentation-neutral base characters
  and punctuation;
- mixed RTL/LTR paragraphs with Latin words, ASCII and Arabic-Indic digits,
  paired brackets and nested isolates;
- emoji modifiers, variation selectors, flags, keycaps and ZWJ sequences;
- font fallback where one grapheme must not be split across fonts;
- line wrapping, tabs, line endings, clipped panes and high-DPI scaling;
- byte-to-cluster, point-to-byte, cursor and multi-run selection mapping;
- repeated font load/close, resize, device reset, cache eviction and teardown.

Tests assert logical byte preservation and stable mappings; pixel snapshots
are supplemental and must not replace semantic conformance assertions.

## 6. Integration and release gates

- Add SDL3_ttf and FriBidi as immutable, checksum-verified build dependencies.
- Build the GUI-SDL target with those exact dependencies on Linux, macOS and
  Windows; test static/shared packaging intentionally, not accidentally.
- Run all tests under ASan/UBSan where supported, plus the SDL dummy/software
  renderer.
- Install the frontend package and compile a clean-prefix consumer with only
  its exported CMake targets.
- Keep the TTF and bidi stack private to GUI-SDL; no SDL, FreeType, HarfBuzz or
  FriBidi headers may leak through Frontend Core public headers.
- Include third-party license files and font licenses in product packaging.
- Keep IME, rich styles and panels behind their respective accepted contracts.

Until these gates pass, SDL3_ttf/FriBidi remain candidates and GUI-SDL keeps its
explicit ASCII/debug-text limitation. A decoder or a successful font render
smoke alone does not complete Phase 6.
