# Validation follow-up — readiness-driven Protocol client

This record supplements `FOUNDATION-1.0.0-VALIDATION.md`; it does not replace
or retroactively change the earlier Foundation validation snapshot.

## Change

- Added `include/cdin/frontend/core/protocol/async_client.h` and
  `core/src/protocol/async_client.c` as an additive, caller-buffered nonblocking
  Protocol client. The existing blocking client structure and functions remain
  available.
- RX retains partial frame-prefix/body progress. TX retains partial frame
  offsets. Callbacks distinguish `PROGRESS`, `AGAIN`, EOF and failure.
- The client checks frame bounds before accepting the body, then validates
  Protocol version, message direction and envelope sequence.
- TX sequence advances only after the complete encoded frame is written.
- Added an integration test that forces repeated `AGAIN` and two-byte progress
  on both read and write paths.

Spec traceability: [FE-019](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L82),
[FE-001](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L63),
[Frontend Protocol](https://github.com/cdin-editor/spec/blob/main/docs/architecture/frontend-protocol.md).

## Verification

| Configuration | Result |
|---|---:|
| GCC 14.2 Release, test logger shim, TUI + Headless | 16/16 CTest passed |
| Clang Release, test logger shim, TUI + Headless | 16/16 CTest passed |
| GCC Debug + AddressSanitizer + UndefinedBehaviorSanitizer | 16/16 CTest passed |
| `scripts/check-boundaries.sh` | PASS |

The test logger shim is explicitly test-only. The production `m-mdy-m/log.c`
package was not rebuilt in this environment because the pinned source download
requires network access. No Windows/macOS native run or live Backend/Runtime
transport integration is claimed.

## Remaining FE-019 integration

The new client is a Core primitive, not a transport or wait-set. The `cdin`
composition root still must provide concrete transport callbacks, a blocking
readiness wait that multiplexes transport and terminal handles, and shutdown
callbacks that restore terminal state. The TUI remains a preview library rather
than a runnable editor application.


## Additional semantic and TUI hardening

- Semantic IDs and text are now checked as bounded, valid UTF-8; `document_id`
  is required and non-empty. Atomic commits reject per-view revision regression
  and document-version regression while the view retains the same document.
- The TUI tracks its single outstanding visible-line request by view/version/
  first-line/count, rejects unsolicited or future-version responses, ignores
  stale responses and only clears pending state after a successful flush.
- Tests cover invalid UTF-8, stale semantic revisions, unsolicited/future line
  responses and delayed stale line data.

Spec: [FE-004](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L66),
[FE-005](https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md#L67),
[UI Model v1](https://github.com/cdin-editor/spec/blob/main/docs/architecture/ui-model-v1.md).
