# Contributing

Thank you for helping improve CDIN Frontend. The [CDIN Spec](https://github.com/cdin-editor/spec)
is the source of truth for cross-component behavior. This repository implements
the Frontend side of those contracts; it must not redefine them.

## Before changing code

1. Read [Architecture](docs/architecture/overview.md) and [Module boundaries](docs/architecture/module-boundaries.md).
2. Check the exact requirement and decision in [Traceability](docs/traceability.md).
3. Do not add direct dependencies on Kernel, Runtime, or Backend-private headers.
4. Do not move document, selection, command, keymap or semantic view-state
   ownership into the Frontend. The Frontend may retain only disposable
   protocol and presentation projections.
5. Keep all visual caches bounded and disposable; key them by the versions carried by the Protocol.

## Code style

- Use C11 for the initial native implementation and the repository's strict warning set.
- Prefer small modules with one responsibility and opaque state where practical.
- Check every return status from the Protocol library and platform APIs.
- Keep raw document and paste content as bytes. Never silently coerce byte strings to UTF-8 text.
- Use `CDIN_FRONTEND_*` logging macros. Do not add a second logger or ad-hoc output outside the diagnostics module.
- Keep Core public headers under `include/cdin/frontend/core/`; adapter public headers
  live under that adapter’s `include/cdin/frontend/ui/<adapter>/` tree. Keep
  implementation under the matching `core/src/` or `ui/<adapter>/src/` tree.
- Add integration/E2E coverage for observable behavior and boundary cases; unit tests are useful for small deterministic helpers but are not a substitute for end-to-end flows.

## Verification

```sh
make deps
make check
```

For a focused change:

```sh
make build
make test
make boundary
```

Document any test that cannot run locally and why. Do not claim a platform or GUI adapter is verified unless the corresponding job actually ran.

## Branch flow

After the initial commit on `main`, run `bash scripts/init-git-flow.sh` to
initialize gix and create `develop`. Start work with `gix feature start <name>`
and finish with `gix feature finish <name>` after review and green checks.
