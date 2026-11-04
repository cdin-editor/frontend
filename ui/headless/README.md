# Headless reference adapter

The Headless adapter is display-free and depends only on the Frontend Core
Protocol API. Its first implementation unit is a bounded, deterministic trace
recorder/replayer. It does not open files, sockets, terminals or windows; the
composition root owns persistence and transport construction.

## Trace format v1

A trace begins with the eight-byte `CDHR` header: four magic bytes, a big-endian
16-bit format version (`1`), and two reserved zero bytes. Each record contains a
20-byte header followed by the original CBOR payload:

| Offset | Size | Meaning |
|---:|---:|---|
| 0 | 1 | Direction: Backend-to-Frontend (`1`) or Frontend-to-Backend (`2`) |
| 1 | 1 | Reserved; must be zero |
| 2 | 2 | Protocol major, big-endian |
| 4 | 2 | Protocol minor, big-endian |
| 6 | 2 | Message type, big-endian |
| 8 | 8 | Envelope sequence, big-endian |
| 16 | 4 | Payload byte length, big-endian |
| 20 | N | Original validated CBOR payload bytes |

The recorder is caller-buffered and performs no allocation. It rejects invalid
protocol payloads, unsupported versions, wrong message directions, non-core
namespaces and capacity exhaustion. Replay validates every record before
calling the supplied callback and can filter by direction. Truncated records,
unknown trace versions and callback failures are reported explicitly.

## Example

```c
uint8_t trace_bytes[64 * 1024];
cdin_frontend_headless_recorder recorder;

if (cdin_frontend_headless_recorder_init(
        &recorder, trace_bytes, sizeof(trace_bytes)) != CDIN_FRONTEND_HEADLESS_OK) {
    return 1;
}

/* Call record() for each complete validated message observed by the peer. */
/* Persist recorder.storage[0..recorder.length] in the composition root. */
```

## Scope and limitations

This module provides the deterministic trace primitive required by FE-007; it
is not yet the complete reference peer. The composition root must still wire
recording to every received message and replay scripted input through the
normal Frontend Protocol client. The integration harness must compare replay
outputs against shared Spec conformance vectors before FE-007 can be marked
fully implemented.
