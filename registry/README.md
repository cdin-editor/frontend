# Registry release metadata

Frontend publishes variant-specific Release Metadata in `registry/releases/`:

- `frontend-tui-1.0.0.json`: `frontend/tui`, package version `1.0.0`
- `frontend-gui-sdl-1.0.0.json`: `frontend/gui-sdl`, package version `1.0.0`

Both variants currently reference the same immutable source revision `1373715c23eb965445caa926de3a99d9c0246f81`. Both require `org.cdin.frontend-protocol` version `1.0.0`; Backend provides that contract.

CDIN's TUI product selects the TUI variant. Its GUI-SDL product selects both TUI and GUI-SDL variants so the composed executable retains the TUI fallback while adding the SDL adapter. Future releases must publish new immutable metadata documents with full source commits and accurate Protocol compatibility. Registry is a build-time resolver/source collector, never a Frontend runtime dependency.
