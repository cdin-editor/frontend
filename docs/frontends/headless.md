# Headless — Protocol reference adapter

**Spec status:** accepted reference behavior (FE-007). **Implementation
status:** partial. The bounded trace recorder/replayer exists in
`ui/headless/src/recorder.c`; it is a frontend-owned primitive, not yet the
complete live reference peer described below. The Backend's separate headless
harness is not a substitute for this adapter.

## 1. Purpose

Headless has no terminal, window, font, renderer or display dependency. It is
the canonical Frontend-side adapter for observing Protocol behavior,
recording all Backend messages and replaying scripted frontend input.

It uses the same Core client/session/model APIs as TUI and GUI. It must not
create a second protocol implementation or bypass handshake, sequencing,
capability negotiation, driver permissions, flush commits or ACK behavior.

## 2. Responsibilities

- perform `hello`, `hello_ack`, `attach` and `detach` like a real adapter;
- record every received message in connection order, including errors and
  `ui.flush` boundaries;
- record or replay frontend-originated input and response messages;
- preserve raw CBOR frames and byte-string payloads without UTF-8 conversion;
- stage snapshots/responses and assert atomic commit behavior;
- ACK only after a valid group is committed;
- request version-pinned visible line ranges when the test scenario requires
  them;
- provide deterministic reports for sequence, version, capability and
  permission violations.

## 3. Recording and replay design

The recorder must retain the original frame bytes as well as enough
connection metadata to interpret sequence, direction, negotiated version and
flush boundaries. Human-readable summaries are useful, but they are not a
replacement for the exact binary frame because document/paste content may
contain arbitrary bytes.

The initial binary trace format is versioned as `CDHR` v1 and stores protocol
message direction, wire version, type, sequence and the original CBOR payload.
The format is bounded by caller-provided storage and rejects malformed or
truncated records. It records validated message payloads rather than full
framing bytes; if byte-for-byte frame capture becomes necessary, the trace
format must be extended with an explicit version bump.

Replay inputs must be explicit: frame bytes, requested capabilities,
connection events and optional deterministic readiness outcomes. Tests must
not rely on ambient wall-clock timing. ESC/readiness timeout cases should use a
controllable fake clock or synthetic timeout event.

## 4. Conformance modes

Headless should support at least:

1. **Codec mode:** feed a frame, validate it, report the decoded message and
   schema result.
2. **Client mode:** run hello/attach, exchange messages over an in-memory
   callback transport and record them.
3. **Session mode:** simulate driver/observer attachment order, detach and
   promotion.
4. **Model mode:** apply valid/invalid snapshots and response groups, test
   revision and document-version behavior.
5. **Replay mode:** replay an ordered transcript and compare the resulting
   message trace with an expected trace.
6. **Fault mode:** inject partial reads/writes, `AGAIN`, EOF, malformed frames,
   queue pressure, wrong sequence and reconnect.

## 5. Output and determinism

- Preserve exact original input and line bytes.
- Produce stable summaries independent of process address or pointer values.
- Do not include wall-clock timestamps in deterministic comparisons unless
  the test explicitly tests timestamp formatting.
- Keep every recording, pending frame and model cache under a configured cap.
- Make truncation/refusal explicit; never silently discard recorded messages.
- Keep secrets and real clipboard/dialog content out of default fixtures.

## 6. Implementation structure

```text
ui/headless/
├── include/cdin/frontend/ui/headless/
└── src/
    ├── recorder/    # bounded raw frame/message log
    ├── replay/      # deterministic script runner
    ├── assertions/  # trace/model/sequence checks
    └── transport/   # in-memory partial-I/O test transport
```

The adapter links Core only. It must run with no SDL, terminal, font, Kernel,
Runtime or Backend-private dependency.

## 7. Acceptance tests

- identical fixture yields byte-identical transcript summary;
- input line/paste bytes including NUL and malformed UTF-8 round-trip exactly;
- snapshots apply only at `ui.flush`; response flush preserves snapshot;
- ACK uses flush-group sequence, not envelope `seq`;
- stale lines are rejected and visible ranges requested again;
- null layout/active view and empty options are handled under Protocol 1.0.0;
- driver-only messages are blocked for observers;
- unknown optional fields and capability identifiers follow Spec behavior;
- partial I/O, `AGAIN`, EOF and malformed input produce deterministic outcomes;
- recording cap produces an explicit failure/report, not silent truncation.
