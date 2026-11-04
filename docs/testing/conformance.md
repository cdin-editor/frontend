# Protocol conformance

The normative Frontend Protocol contract is maintained in
[CDIN Spec](https://github.com/cdin-editor/spec/blob/main/docs/architecture/protocol-wire.md).
The Backend reference codec/server provides the sibling implementation and
its executable tests. Frontend tests must consume the same published vectors;
this repository must not create a fork with different expected values.

## Version gate

- Current Frontend source: Protocol 1.0.0.
- Current paired Backend source: Protocol 1.0.0.
- Both local codec suites now cover targeted positive and negative regressions
  for required typed options and nullable PaneTree `active_view`.
- A complete cross-repository conformance claim still requires the Spec-owned
  vector corpus and full message-by-message execution on both codecs.

## Required vector groups

1. **Envelope and CBOR:** shortest encodings, definite-length items, exact
   envelope fields, duplicate-key rejection, malformed UTF-8 text, no floats,
   no trailing bytes and no indefinite-length values.
2. **Byte preservation:** NUL, invalid UTF-8, BOM, CR/LF/CRLF and arbitrary
   paste bytes remain CBOR byte strings and round-trip exactly.
3. **Version negotiation:** valid and incompatible major/minor ranges,
   malformed hello/hello_ack and attach-before-negotiation.
4. **Messages:** every inbound/outbound schema, direction restrictions,
   required fields, optional fields, unknown core message and optional
   namespace/capability handling.
5. **Protocol 1.0.0 model changes:** nullable `active_view`, null root/active
   leaf, required-but-empty typed options map, recognized option types/ranges,
   recursive PaneTree validation and bounded view/panel counts.
6. **Role and sequence:** one driver, observer restrictions, FIFO promotion,
   envelope sequence, flush-group sequence and ACK validation.
7. **Atomic model:** snapshot replacement, response preservation, removed
   panels/views, invalid group rollback and ACK only after commit.
8. **Line windows:** request version, total line count, byte spans, stale
   responses, invalidation and visible-range re-request.
9. **Capabilities:** registered, requested, accepted and adapter-supported
   states remain distinct.
10. **Transport:** partial reads/writes, readiness, `AGAIN`, EOF, error,
    truncated frame and bounded resource behavior.

## How to add support

1. Locate the normative schema/requirement in the Spec.
2. Clarify an ambiguous contract in Spec before writing a local expected value.
3. Add/extend the shared vector in the canonical vector source when one is
   published; never update only one implementation's expectation.
4. Run it against Frontend and Backend implementations.
5. Add adapter integration tests only for adapter-specific presentation,
   cell/pixel mapping or lifecycle behavior.
6. Update `docs/traceability.md` with source/test anchors and status.

The repositories currently have targeted mirrored local tests but no complete
Spec-owned 1.0.0 vector suite. The absence of the full shared corpus is an
integration blocker; the local paired regressions are not a substitute for
message-by-message cross-implementation conformance.
