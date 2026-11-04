# Frontend diagnostics

## 1. Logger ownership

Frontend uses the standalone [m-mdy-m/log.c](https://github.com/m-mdy-m/log.c)
C11 logging library. Each component creates or receives its own
`mmlog_logger_t` instance through `cdin_frontend_logger_create()` and owns the
lifetime of that instance. Frontend does not include or call the Kernel logger,
Backend diagnostics or a hidden process-global logger.

The `cdin` composition root creates logger instances, configures severity and
output policy, passes them to component constructors, and destroys them only
after the component has stopped all workers.

## 2. Record format

```text
[FRONTEND][PROTOCOL] client.c:142 receive(): rejected sequence 12 (expected 11)
```

The record also carries `layer=cdin`, a Frontend component tag, source file,
line, function and severity. The text prefix remains stable and searchable.

## 3. Severity

| Level | Value | Intended use |
| --- | ---: | --- |
| TRACE | 0 | Per-frame/per-message details; very verbose. |
| DEBUG | 1 | State transitions and diagnostic context. |
| INFO | 2 | Startup, attach and lifecycle milestones. |
| WARN | 3 | Recoverable invalid input, stale data or pressure. |
| ERROR | 4 | Operation failed; current request cannot continue. |
| FATAL | 5 | Component cannot safely continue. |

The caller selects the minimum level. Logging is best-effort and must not
become a new failure path. A null logger is permitted for embedders that
disable diagnostics; production `cdin` should pass an explicitly configured
logger to every component.

## 4. Areas

Use stable uppercase tags:

- `CORE`: shared initialization and lifecycle;
- `PROTOCOL`: codec, handshake, sequencing, transport and ACKs;
- `SESSION`: attach/detach and driver/observer state;
- `MODEL`: semantic revisions, view metadata, line cache and invalidation;
- `CAPABILITIES`: registry and negotiated feature state;
- `INPUT`: physical event translation;
- `LAYOUT`: adapter-owned visual geometry and motion;
- `TEXT`: Unicode, shaping, bidi and display-width mapping;
- `RENDER`: adapter drawing/output;
- `PLATFORM`: terminal/window handles and OS failures.

`LAYOUT`, `TEXT`, `RENDER` and `PLATFORM` are adapter-local areas, not Core
presentation modules. Avoid logging entire document/paste bytes or sensitive
clipboard/dialog contents by default.
