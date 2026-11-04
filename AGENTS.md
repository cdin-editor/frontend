# Repository instructions

- The Spec is authoritative; do not invent Frontend-only protocol extensions.
- Keep Frontend Core under `core/` UI-neutral. TUI, GUI-SDL and Headless are
  peer implementations under `ui/`; they may depend on Core but never on each
  other. Public TUI headers live under `ui/tui/include/`.
- The current source implements shared Protocol 1.0.0. Keep changes aligned with
  `docs/contracts/protocol-1.0.0.md` and the Backend mirror. Add matching positive
  and negative test vectors to both codecs when a wire schema changes. Do not
  conflate Core codec conformance with full Runtime/transport integration. Frontend owns its independent codec and
  depends only on its public contracts, the standalone `m-mdy-m/log.c` logger,
  and each UI own platform/toolkit APIs. Never include Kernel, Runtime, or Backend-private headers.
- Do not store canonical documents, selection state, commands, keymaps or
  workspace policy in Frontend modules.
- Keep visual layout, shaping, bidi, text presentation and rendering state
  inside the selected UI adapter; do not add a shared GUI/TUI rendering or
  text-layout engine to Core. Keep all such caches bounded and disposable.
- Preserve arbitrary document/paste bytes exactly.
- Route native diagnostics through `cdin_frontend_logf` and the
  `CDIN_FRONTEND_*` macros. Direct console output is restricted to the
  diagnostics implementation and explicit command-line tools/tests.
- Run `make check` and document any environment-specific limitation.
