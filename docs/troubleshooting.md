# Troubleshooting

## Start with Frontend diagnostics

Every record begins with `[FRONTEND][AREA]` and includes source file, line,
function and severity. Examples:

```text
[FRONTEND][PROTOCOL] client.c:142 receive(): rejected sequence 12 (expected 11)
[FRONTEND][MODEL] model.c:208 apply_lines(): discarded stale version 7 (current 8)
```

Use stable areas: `CORE`, `PROTOCOL`, `SESSION`, `MODEL`, `CAPABILITIES`,
`INPUT`, `LAYOUT`, `TEXT`, `RENDER` and `PLATFORM`. Logs must not become a
failure path or write ad-hoc diagnostic output from production modules.

## Protocol version problems

| Symptom | Check |
| --- | --- |
| `VERSION_MISMATCH` | Current source implements Protocol 1.0.0; peers must negotiate the shared major/minor. Both peers must support the same negotiated version. |
| 1.0.0 hello succeeds but model is rejected | Verify both peers use the required nullable `active_view` and typed `ui.options.options`; check the Runtime patch and shared message-schema constraints. |
| Frame rejected | Check 4-byte big-endian length, body limit, deterministic CBOR, duplicate keys, namespace, required fields and message direction. |
| ACK rejected | Confirm it acknowledges `ui.flush.payload.sequence`, not the envelope message `seq`. |
| Update appears partially | Do not render before `ui.flush`; verify the whole group stages and commits atomically. |
| Old line appears after edit | Compare view ID and document version; discard stale results and request the current visible range. |
| Observer input is rejected | Only the driver may send mutating events. Wait for driver promotion or reconnect. |
| Clipboard/dialog/canvas unavailable | Check `hello_ack.accepted_capabilities`; requested does not mean accepted. |

## TUI problems

| Symptom | Check |
| --- | --- |
| Arrow keys appear as Escape | Preserve partial CSI/SS3 input and use the ESC disambiguation timeout. |
| Bracketed paste is not recognized | Confirm terminal support and that open/close enables/disables the mode. |
| Terminal remains in raw mode | Ensure the orderly cleanup path calls the platform close routine; process signals must only request cleanup. |
| Backend updates stop while typing | The application loop must multiplex terminal and transport readiness; a blocking full-frame read/write is not sufficient. |
| Cursor is misplaced on Persian/wide text | Inspect byte-to-grapheme-to-cell mapping and terminal emulator behavior; do not equate byte count with cell width. |
| Pane is blank after snapshot | Check nullable `active_view`, whether the view exists in the leaf's list, and whether line windows were requested for the right version. |

## Build/dependency problems

- Run `make deps` and verify the pinned SHA in `deps.lock` before replacing
  `m-mdy-m/log.c`; do not silently substitute `main`.
- Never build release artifacts with `CDIN_FRONTEND_USE_LOG_STUB=ON`.
- `make build-gui` requires SDL3 3.2+ development files discoverable by CMake;
  `make run-gui` launches only the current desktop smoke preview.
- A Linux test run does not validate Windows terminal/console behavior.

## Known gaps

The source is Protocol 1.0.0 with a runnable read-only TUI preview and an
optional SDL3 GUI smoke preview. Neither is a connected editor executable.
Headless live-peer wiring, complete GUI-SDL, 1.0.0 full-message conformance,
platform event-loop readiness, multi-pane UI, full Unicode text presentation,
concrete transport and live end-to-end integration are still missing. See [Status](status.md).
