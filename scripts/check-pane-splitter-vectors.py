#!/usr/bin/env python3
"""Validate Frontend coverage mapping for the accepted PaneTree splitter vectors.

The fixture remains authoritative in cdin-editor/spec. This mirrored copy is
checked for exact identity and every Frontend-owned scenario is mapped to a
strict Core/GUI regression that executes under CTest. Backend/Runtime-owned
split-ID existence checks are verified by the Backend/Runtime shared runner;
Frontend deliberately does not duplicate semantic tree authorization.
"""
from __future__ import annotations

import hashlib
import json
import pathlib
import sys

# This is the Git blob SHA-1 for Spec's accepted fixture at commit
# 274f84f0ca08378db9c4a5cf6820065256f31d5a. The mirrored file must remain
# byte-for-byte identical; updating the Spec fixture requires an intentional
# pin change here.
SPEC_FIXTURE_GIT_BLOB_SHA1 = "16477a16ecf65795eaac17063e4ac6ba288d0408"

REQUIRED_IDS = {
    "horizontal-split-commit",
    "nested-vertical-split-commit",
    "minimum-and-maximum-ratios",
    "ratio-below-minimum-rejected",
    "ratio-above-maximum-rejected",
    "stale-layout-revision-rejected",
    "unknown-split-rejected",
    "leaf-id-is-not-a-split-id",
    "observer-cannot-resize",
    "capability-absent-does-not-send-extension",
    "cancel-before-release-does-not-mutate",
    "revision-change-during-drag-cancels-preview",
    "success-result-precedes-atomic-snapshot",
}

LOCAL_EVIDENCE = {
    "horizontal-split-commit": (
        "tests/ui/gui-sdl/integration/adapter_smoke.c",
        "smoke_test_splitter_lifecycle",
    ),
    "nested-vertical-split-commit": (
        "tests/ui/gui-sdl/integration/geometry.c",
        "CDIN_FRONTEND_GUI_SDL_LAYOUT_VERTICAL",
    ),
    "minimum-and-maximum-ratios": (
        "tests/core/unit/protocol_wire.c",
        "inclusive maximum",
    ),
    "ratio-below-minimum-rejected": (
        "tests/core/unit/protocol_wire.c",
        "pane_resize_bad_ratio",
    ),
    "ratio-above-maximum-rejected": (
        "tests/core/unit/protocol_wire.c",
        "9000: rejected",
    ),
    "stale-layout-revision-rejected": (
        "tests/ui/gui-sdl/integration/adapter_smoke.c",
        "changed committed revision cancels an in-progress preview",
    ),
    "observer-cannot-resize": (
        "tests/core/integration/session_state.c",
        "was_driver",
    ),
    "capability-absent-does-not-send-extension": (
        "tests/ui/gui-sdl/integration/adapter_smoke.c",
        "smoke_test_no_splitter_capability",
    ),
    "cancel-before-release-does-not-mutate": (
        "tests/ui/gui-sdl/integration/adapter_smoke.c",
        "SDL_EVENT_WINDOW_FOCUS_LOST",
    ),
    "revision-change-during-drag-cancels-preview": (
        "tests/ui/gui-sdl/integration/adapter_smoke.c",
        "5500u, 3u",
    ),
    "success-result-precedes-atomic-snapshot": (
        "tests/ui/gui-sdl/integration/adapter_smoke.c",
        "CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESULT",
    ),
}
BACKEND_RUNTIME_CASES = {
    "unknown-split-rejected",
    "leaf-id-is-not-a-split-id",
}

def fail(message: str) -> None:
    raise SystemExit(f"pane-splitter vectors: {message}")

def main() -> None:
    if len(sys.argv) != 3:
        fail("usage: check-pane-splitter-vectors.py FIXTURE FRONTEND_ROOT")
    fixture_path = pathlib.Path(sys.argv[1])
    root = pathlib.Path(sys.argv[2])
    data = json.loads(fixture_path.read_text(encoding="utf-8"))

    if data.get("schema") != "cdin.frontend-protocol.conformance.v1":
        fail("schema mismatch")
    if data.get("family") != "proto.pane-splitter" or data.get("status") != "ACCEPTED":
        fail("family/status mismatch")
    if data.get("protocol_version") != [1, 0]:
        fail("Protocol version mismatch")
    if data.get("capability") != "pane-splitter-resize/1":
        fail("capability mismatch")
    if data.get("optional_namespace") != "pane":
        fail("optional namespace mismatch")
    if data.get("ratio_bp") != {"min": 1000, "max": 8999}:
        fail("basis-point bounds mismatch")

    cases = data.get("cases")
    if not isinstance(cases, list):
        fail("cases must be an array")
    by_id = {item.get("id"): item for item in cases}
    if len(by_id) != len(cases):
        fail("duplicate vector IDs")
    if set(by_id) != REQUIRED_IDS:
        fail(f"case inventory mismatch: missing={sorted(REQUIRED_IDS - set(by_id))}, "
             f"extra={sorted(set(by_id) - REQUIRED_IDS)}")

    for case_id, (relative_path, marker) in LOCAL_EVIDENCE.items():
        source = root / relative_path
        if not source.is_file():
            fail(f"{case_id}: missing local test source {relative_path}")
        if marker not in source.read_text(encoding="utf-8"):
            fail(f"{case_id}: executable regression mapping missing")

    for case_id in BACKEND_RUNTIME_CASES:
        expect = by_id[case_id].get("expect", {})
        if expect.get("result") != "not_found" or expect.get("tree_unchanged") is not True:
            fail(f"{case_id}: expected Backend/Runtime failure-atomic behavior")

    ratios = by_id["minimum-and-maximum-ratios"].get("requests", [])
    if [item.get("ratio_bp") for item in ratios] != [1000, 8999]:
        fail("inclusive ratio endpoint vectors changed")
    checks = (
        ("ratio-below-minimum-rejected", 999),
        ("ratio-above-maximum-rejected", 9000),
    )
    for case_id, expected in checks:
        actual = by_id[case_id].get("request", {}).get("payload", {}).get("ratio_bp")
        if actual != expected:
            fail(f"{case_id}: expected ratio_bp={expected}, got {actual}")

    raw_fixture = fixture_path.read_bytes()
    git_blob_header = f"blob {len(raw_fixture)}\\0".encode("ascii")
    git_blob_sha1 = hashlib.sha1(git_blob_header + raw_fixture).hexdigest()
    if git_blob_sha1 != SPEC_FIXTURE_GIT_BLOB_SHA1:
        fail(
            "fixture differs from the pinned Spec ADR-0024 vector "
            f"(expected git blob {SPEC_FIXTURE_GIT_BLOB_SHA1}, got {git_blob_sha1})"
        )
    digest = hashlib.sha256(raw_fixture).hexdigest()
    print(
        f"validated {len(cases)} accepted splitter vectors; "
        f"spec_blob={git_blob_sha1}; sha256={digest}"
    )

if __name__ == "__main__":
    main()
