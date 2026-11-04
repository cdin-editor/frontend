# CLI — non-interactive client (deferred)

The Spec distinguishes `cli` from `tui`:

- **TUI** is an interactive, full-screen terminal editor and the first
  interactive Frontend target (FE-009/FE-011).
- **CLI** is a non-interactive command client that attaches to a session,
  submits commands and prints results (FE-008).

FE-008 remains `DEFERRED`. This repository must not call the TUI a CLI or
silently combine the two products. When CLI work is approved, it should be a
separate peer under `ui/cli/`, use Core Protocol/session APIs, and depend on no
TUI code. Its command vocabulary remains Runtime-owned; it is a transport and
output adapter, not a second command interpreter.
