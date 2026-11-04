# Verification strategy

## 1. Verification layers

| Layer | What it proves | Examples |
| --- | --- | --- |
| Unit | One Core/adapter function obeys its local contract | CBOR decoder, schema, state transition, parser, byte-to-cell mapping |
| Integration | Several modules preserve an invariant across their public APIs | Client over partial I/O, handshake, flush/ACK, terminal lifecycle |
| Shared conformance | Frontend and Backend interpret the same normative vectors identically | Protocol envelopes, messages, versions, limits, malformed frames |
| Adapter acceptance | Environment-specific input/presentation behavior | PTY cells, SDL pixel geometry, font/bidi, resize and focus |
| Product E2E | A live editor operation traverses the complete product path | Open, insert, undo, redo, save, reopen and byte-compare |
| Stress/fault | Limits and cleanup survive pressure and failure | queue full, large frames, EOF, `AGAIN`, allocation failure, disconnect |

A green unit suite must not be described as full Protocol, TUI, GUI or product
conformance.

## 2. Core Protocol and model cases

- partial four-byte prefix and partial CBOR body reads;
- partial writes and resume at the exact byte offset;
- `AGAIN` distinct from EOF and fatal error;
- zero/over-limit frame length, truncation, trailing bytes and deep nesting;
- invalid UTF-8 text versus valid arbitrary byte strings;
- duplicate map keys, non-shortest CBOR, indefinite values and floats;
- wrong direction, unknown message, version mismatch and sequence rollback;
- attach before negotiation, reconnect and detach;
- driver mutation versus observer read-only operation;
- requested versus accepted versus implemented capabilities;
- snapshot/response semantics, ACK after commit, null fields and empty options;
- 32-view/16-panel caps, stale versions, byte spans and line-window limits.

## 3. TUI tests

- terminal open refuses redirected/non-interactive streams without changing
  terminal state;
- raw mode, alternate screen, cursor and bracketed paste are restored exactly;
- partial UTF-8, CSI, SS3, Escape disambiguation and bracketed paste;
- terminal size changes during pending Backend frames;
- terminal and transport both ready without starvation;
- tabs, combining marks, wide glyphs, grapheme clusters and invalid bytes;
- cursor and selection map to correct byte/cell positions;
- pane tree resize, empty leaf and null active view;
- rendering output handles partial writes and output-buffer limit;
- signal/error/EOF path restores terminal modes.

## 4. GUI-SDL tests

When implementation exists:

- SDL subsystem/window/renderer creation and every partial-failure cleanup;
- scale/DPI and resize behavior;
- key, committed text/IME, paste, mouse, focus and capability mapping;
- font fallback, shaped cluster to byte mapping, bidi and cursor hit testing;
- pixel layout of recursive pane tree and panels;
- texture/glyph-cache memory bounds and invalidation;
- device loss/window close/transport disconnect teardown.

## 5. Headless tests

- deterministic record/replay output;
- raw bytes preserved in transcript;
- all message ordering, driver and ACK cases;
- malformed transcript handling and bounded recorder behavior;
- simulated partial I/O, `AGAIN`, EOF, timeout and reconnect.

## 6. Platform matrix

The current source has POSIX and Windows terminal adapters. Locally verified
Linux tests do not prove Windows behavior. CI must run the real dependency
build and platform jobs for every OS the release claims to support. The GUI
adds SDL/font package availability and native event integration as separate
gates. macOS must be tested before it is listed as supported.

## 7. Product E2E acceptance

At least one live path must verify:

1. `cdin` starts Backend and supplies the transport;
2. Frontend negotiates the same Protocol version as Backend;
3. Runtime publishes a semantic UI snapshot;
4. Frontend applies it atomically and ACKs the correct flush sequence;
5. Frontend sends raw key/text input as the driver;
6. Runtime command/keymap policy performs the edit through Backend/Kernel;
7. new document version invalidates the old line cache;
8. Frontend fetches/render lines pinned to the new version;
9. save/reopen preserves exact bytes.

Do not claim the product is an integrated runnable editor before this path
passes.
