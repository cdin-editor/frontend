# End-to-end tests

End-to-end scenarios must run a real Frontend adapter against Backend through
a concrete transport supplied by the `cdin` composition root, with Runtime and
Kernel behind Backend. This directory is reserved until that integration path
exists.

Current public-codec and client/session integration tests live under
`tests/core/integration/`. TUI adapter tests live under
`tests/ui/tui/integration/`. They are not product E2E tests.
